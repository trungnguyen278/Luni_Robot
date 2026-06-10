#include "SpiBridge.hpp"
#include "config/ServerConfig.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "SpiBridge";

// Static handshake pin for SPI slave post_setup callback.
// Set HIGH after DMA is loaded, so master sees handshake only when slave is ready.
static gpio_num_t s_handshake_pin = GPIO_NUM_NC;
static bool s_handshake_pending = false;

static void IRAM_ATTR spi_post_setup_cb(spi_slave_transaction_t* trans)
{
    // Set handshake to accurately reflect data availability.
    // HIGH = this frame has real data OR more data waiting.
    // LOW = sending EMPTY and nothing else queued.
    // This fires AFTER DMA is loaded, so master won't clock garbage.
    gpio_set_level(s_handshake_pin, s_handshake_pending ? 1 : 0);
}

SpiBridge::~SpiBridge()
{
    stop();
    if (dl_audio_sb_) { vStreamBufferDelete(dl_audio_sb_); dl_audio_sb_ = nullptr; }
}

bool SpiBridge::init(const Config& cfg)
{
    cfg_ = cfg;

    // Init handshake GPIO as output (LOW = no data)
    gpio_config_t hs_cfg = {};
    hs_cfg.pin_bit_mask = (1ULL << cfg_.pin_handshake);
    hs_cfg.mode = GPIO_MODE_OUTPUT;
    hs_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    hs_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&hs_cfg);
    gpio_set_level(cfg_.pin_handshake, 0);

    // Init SPI slave
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = cfg_.pin_mosi;
    bus_cfg.miso_io_num = cfg_.pin_miso;
    bus_cfg.sclk_io_num = cfg_.pin_sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = spi_proto::FRAME_SIZE;

    s_handshake_pin = cfg_.pin_handshake;

    spi_slave_interface_config_t slave_cfg = {};
    slave_cfg.mode = 0;
    slave_cfg.spics_io_num = cfg_.pin_cs;
    slave_cfg.queue_size = 1;
    slave_cfg.flags = 0;
    slave_cfg.post_setup_cb = spi_post_setup_cb;

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

    ESP_LOGI(TAG, "SpiBridge init OK (MOSI=%d MISO=%d SCLK=%d CS=%d HS=%d)",
             cfg_.pin_mosi, cfg_.pin_miso, cfg_.pin_sclk, cfg_.pin_cs,
             cfg_.pin_handshake);
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
        gpio_set_level(cfg_.pin_handshake, 0);
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

    size_t sent = xStreamBufferSend(dl_audio_sb_, data, len, 0);
    if (sent > 0) {
        gpio_set_level(cfg_.pin_handshake, 1);
    }
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
            ESP_LOGW(TAG, "Bad opus header in stream: %u, skipping", frame_len);
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
    ESP_LOGI(TAG, "Slave task started");

    // DMA-capable buffers
    uint8_t* tx_buf = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);
    uint8_t* rx_buf = (uint8_t*)heap_caps_malloc(spi_proto::FRAME_SIZE, MALLOC_CAP_DMA);

    if (!tx_buf || !rx_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffers");
        free(tx_buf);
        free(rx_buf);
        vTaskDelete(nullptr);
        return;
    }

    while (started_) {
        // Prepare TX frame: control/status > audio downlink > EMPTY
        prepareTxFrame(tx_buf);

        // Tell post_setup_cb whether to raise handshake after DMA is loaded.
        // The callback fires inside spi_slave_transmit, AFTER DMA registers
        // are configured but BEFORE the master clocks — eliminating the race
        // where master sees handshake HIGH but slave DMA isn't ready yet.
        s_handshake_pending =
            (dl_audio_sb_ && xStreamBufferBytesAvailable(dl_audio_sb_) > 0);

        spi_slave_transaction_t t = {};
        t.length = spi_proto::FRAME_SIZE * 8;
        t.tx_buffer = tx_buf;
        t.rx_buffer = rx_buf;

        // Block forever — no timeout. This prevents stale transactions from
        // piling up in the hardware queue (size=1). When timeout was used,
        // the timed-out transaction stayed pending, blocking the next call
        // and causing cascading deadlocks.
        // Producer (sendAudioDownlink) sets handshake HIGH
        // to wake S3, which polls and completes this transaction.
        // S3 also polls when it has uplink data (audio/control).
        esp_err_t err = spi_slave_transmit(SPI2_HOST, &t, portMAX_DELAY);
        if (err == ESP_OK) {
            handleRxFrame(rx_buf);

            // Only pull handshake LOW when buffer is empty. Keeping it HIGH
            // while data is pending prevents S3 from entering its 5ms idle
            // poll delay between back-to-back transactions.
            if (!dl_audio_sb_ || xStreamBufferBytesAvailable(dl_audio_sb_) == 0) {
                gpio_set_level(cfg_.pin_handshake, 0);
            }

            taskYIELD();
        }
    }

    free(tx_buf);
    free(rx_buf);
    ESP_LOGW(TAG, "Slave task ended");
    vTaskDelete(nullptr);
}
