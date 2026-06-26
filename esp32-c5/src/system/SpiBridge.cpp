#include "SpiBridge.hpp"
#include "config/ServerConfig.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "SpiBridge";

// Handshake line removed: the S3 master polls instead (GPIO40 became LCD CS).
// The slave stays gapless (2 pre-queued transactions, see slaveLoop) so a poll
// never lands in a re-arm gap — that, not the handshake, is what now prevents
// dropped frames.

SpiBridge::~SpiBridge()
{
    stop();
    if (dl_audio_sb_) { vStreamBufferDelete(dl_audio_sb_); dl_audio_sb_ = nullptr; }
}

bool SpiBridge::init(const Config& cfg)
{
    cfg_ = cfg;

    // No handshake GPIO (S3 polls; GPIO40 freed for LCD CS).

    // Init SPI slave
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = cfg_.pin_mosi;
    bus_cfg.miso_io_num = cfg_.pin_miso;
    bus_cfg.sclk_io_num = cfg_.pin_sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = spi_proto::FRAME_SIZE;

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.mode = 0;
    slave_cfg.spics_io_num = cfg_.pin_cs;
    slave_cfg.queue_size = 2;   // gapless: keep 2 transactions in flight (see slaveLoop)
    slave_cfg.flags = 0;

    esp_err_t err = spi_slave_initialize(SPI2_HOST, &bus_cfg, &slave_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI slave init failed: %s", esp_err_to_name(err));
        return false;
    }

    dl_audio_sb_ = xStreamBufferCreate(server_cfg::SPI_DL_AUDIO_BUFFER_SIZE, 1);
    if (!dl_audio_sb_) {
        ESP_LOGE(TAG, "DL audio stream buffer create failed");
        return false;
    }

    ESP_LOGI(TAG, "SpiBridge init OK (MOSI=%d MISO=%d SCLK=%d CS=%d, no HS — gapless poll)",
             cfg_.pin_mosi, cfg_.pin_miso, cfg_.pin_sclk, cfg_.pin_cs);
    return true;
}

void SpiBridge::start()
{
    if (started_) return;
    started_ = true;

    // Priority 4 — lower than WS TX task (6) so on single-core C5,
    // WS sends preempt SPI when tx_buffer has data to drain.
    xTaskCreatePinnedToCore(&SpiBridge::slaveTaskEntry, "SpiBridge", 4096, this, 4, &slave_task_, 0);
    ESP_LOGI(TAG, "SpiBridge started");
}

void SpiBridge::stop()
{
    if (!started_) return;
    started_ = false;
    vTaskDelay(pdMS_TO_TICKS(200));
    slave_task_ = nullptr;
    ESP_LOGI(TAG, "SpiBridge stopped");
}

void SpiBridge::resetDownlink()
{
    if (dl_audio_sb_) {
        xStreamBufferReset(dl_audio_sb_);
        ESP_LOGI(TAG, "Downlink buffer reset");
    }
}

bool SpiBridge::sendAudioDownlink(const uint8_t* data, size_t len)
{
    if (!dl_audio_sb_ || len == 0) return false;

    // All-or-nothing: either write ALL bytes or NONE.
    // A partial write breaks the [2B len][opus] byte stream alignment
    // permanently — S3 reads garbage headers → cascade decode errors.
    // Dropping an entire WS message is safe: Opus handles packet loss
    // gracefully (PLC), but corrupted frames cause audible artifacts.
    size_t space = xStreamBufferSpacesAvailable(dl_audio_sb_);
    if (space < len) {
        static uint32_t drop_count = 0;
        drop_count++;
        if (drop_count <= 5 || drop_count % 50 == 0) {
            ESP_LOGW(TAG, "DL audio buffer full, dropped %zu bytes (space=%zu) #%lu",
                     len, space, (unsigned long)drop_count);
        }
        return false;
    }

    // No handshake to raise — the S3 master polls and will pick this up on its
    // next poll. The gapless slave guarantees it won't miss the transaction.
    size_t sent = xStreamBufferSend(dl_audio_sb_, data, len, 0);
    return sent == len;
}

void SpiBridge::handleRxFrame(const uint8_t* rx_buf)
{
    uint8_t msg_type;
    const uint8_t* payload;
    uint16_t payload_len;
    uint8_t seq;

    if (!spi_proto::parseFrame(rx_buf, msg_type, payload, payload_len, seq)) {
        return;
    }

    if (msg_type == (uint8_t)spi_proto::MsgFromS3::AUDIO_UPLINK) {
        if (audio_ul_cb_ && payload_len > 0) {
            audio_ul_cb_(payload, payload_len);
        }
    }
    // HEARTBEAT and CONTROL_CMD: no action needed
    // (control commands now go through UART)
}

// === Prepare next TX frame ===
// Frame-aligned: packs complete [2B len][opus] frames into SPI payload.
// Format: [frame_count:1] [2B len + opus data]... [padding]
// Dropping an entire SPI payload only loses complete opus frames (PLC handles it),
// instead of breaking byte-stream alignment permanently.
void SpiBridge::prepareTxFrame(uint8_t* tx_buf)
{
    if (!dl_audio_sb_ || (xStreamBufferBytesAvailable(dl_audio_sb_) == 0 && !has_pending_header_)) {
        if (ul_space_query_) {
            size_t free_bytes = ul_space_query_();
            uint8_t free_kb = (uint8_t)std::min(free_bytes / 1024, (size_t)255);
            spi_proto::buildFrame(tx_buf, (uint8_t)spi_proto::MsgFromC5::EMPTY,
                                  &free_kb, 1, tx_seq_++);
        } else {
            spi_proto::buildEmptyFrame(tx_buf, tx_seq_++);
        }
        return;
    }

    uint8_t payload[spi_proto::MAX_PAYLOAD];
    size_t pos = 1;   // byte 0 = frame count
    uint8_t frame_count = 0;

    while (pos + 2 < spi_proto::MAX_PAYLOAD) {
        uint8_t header[2];

        if (has_pending_header_) {
            header[0] = pending_header_[0];
            header[1] = pending_header_[1];
            has_pending_header_ = false;
        } else {
            size_t avail = xStreamBufferBytesAvailable(dl_audio_sb_);
            if (avail < 2) break;
            size_t got = xStreamBufferReceive(dl_audio_sb_, header, 2, 0);
            if (got < 2) break;
        }

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 512) {
            // The 2 header bytes are already consumed — the stream is
            // misaligned and every byte after it is garbage. Drop it all;
            // alignment recovers at the next WS message boundary.
            ESP_LOGW(TAG, "Bad opus header in stream: %u — resetting downlink buffer", frame_len);
            xStreamBufferReset(dl_audio_sb_);
            has_pending_header_ = false;
            break;
        }

        size_t needed = 2 + frame_len;
        if (pos + needed > spi_proto::MAX_PAYLOAD) {
            pending_header_[0] = header[0];
            pending_header_[1] = header[1];
            has_pending_header_ = true;
            break;
        }

        size_t avail = xStreamBufferBytesAvailable(dl_audio_sb_);
        if (avail < frame_len) {
            pending_header_[0] = header[0];
            pending_header_[1] = header[1];
            has_pending_header_ = true;
            break;
        }

        payload[pos++] = header[0];
        payload[pos++] = header[1];
        size_t got = xStreamBufferReceive(dl_audio_sb_, &payload[pos], frame_len, 0);
        pos += got;
        frame_count++;
    }

    if (frame_count > 0) {
        payload[0] = frame_count;
        spi_proto::buildFrame(tx_buf, (uint8_t)spi_proto::MsgFromC5::AUDIO_DOWNLINK,
                              payload, (uint16_t)pos, tx_seq_++);
    } else {
        spi_proto::buildEmptyFrame(tx_buf, tx_seq_++);
    }
}

// === Slave Task ===

void SpiBridge::slaveTaskEntry(void* arg)
{
    static_cast<SpiBridge*>(arg)->slaveLoop();
}

void SpiBridge::slaveLoop()
{
    ESP_LOGI(TAG, "Slave task started (gapless, no handshake)");

    // Two transaction slots kept in flight at all times. While one slot is being
    // clocked by the master or refilled here, the OTHER stays armed in HW — so the
    // S3 master (which now POLLS instead of waiting on a handshake line) can clock
    // at any instant without landing in a re-arm gap. A gap would lose that frame
    // in BOTH directions; the old single-transaction loop relied on the handshake
    // to keep the master out of the gap, but GPIO40 became the LCD CS.
    constexpr int N = 2;
    uint8_t* tx_buf[N] = {};
    uint8_t* rx_buf[N] = {};
    spi_slave_transaction_t trans[N] = {};

    for (int i = 0; i < N; ++i) {
        tx_buf[i] = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);
        rx_buf[i] = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);
        if (!tx_buf[i] || !rx_buf[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffers");
            for (int j = 0; j < N; ++j) { free(tx_buf[j]); free(rx_buf[j]); }
            vTaskDelete(nullptr);
            return;
        }
    }

    // Prime both slots so the HW always has a transaction armed from the start.
    for (int i = 0; i < N; ++i) {
        prepareTxFrame(tx_buf[i]);
        trans[i].length    = spi_proto::FRAME_SIZE * 8;
        trans[i].tx_buffer = tx_buf[i];
        trans[i].rx_buffer = rx_buf[i];
        spi_slave_queue_trans(SPI2_HOST, &trans[i], portMAX_DELAY);
    }

    while (started_) {
        // Wait for the master to clock the oldest armed transaction. The peer slot
        // stays armed during this wait → the bus is always covered (no gap).
        spi_slave_transaction_t* done = nullptr;
        if (spi_slave_get_trans_result(SPI2_HOST, &done, portMAX_DELAY) != ESP_OK || !done)
            continue;

        int idx = (int)(done - trans);   // which slot completed (0 or 1)
        handleRxFrame(rx_buf[idx]);

        // Refill this slot's TX and re-queue it; the peer slot keeps the bus
        // covered while we touch this one → still gapless.
        prepareTxFrame(tx_buf[idx]);
        done->length = spi_proto::FRAME_SIZE * 8;
        spi_slave_queue_trans(SPI2_HOST, done, portMAX_DELAY);
    }

    for (int i = 0; i < N; ++i) { free(tx_buf[i]); free(rx_buf[i]); }
    ESP_LOGW(TAG, "Slave task ended");
    vTaskDelete(nullptr);
}
