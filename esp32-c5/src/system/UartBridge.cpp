#include "UartBridge.hpp"
#include "esp_log.h"
#include <cstring>
#include <cstdlib>

static const char* TAG = "UartBridge";

UartBridge::~UartBridge()
{
    stop();
}

bool UartBridge::init(const Config& cfg)
{
    cfg_ = cfg;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = cfg_.baud_rate;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    // RX ring stays 16 KB: the C5 has no spare heap to hold a whole image here —
    // a 40 KB ring dropped the largest free block to ~22 KB and broke the mbedTLS
    // handshake (esp-aes alloc fail). Instead the burst is paced on the S3 side,
    // which buffers the JPEG in its 8 MB PSRAM and feeds chunks at the rate the
    // C5 can forward over WS (~1 chunk/RTT, Nagle-limited). With pacing the ring
    // never fills, so 16 KB is plenty. TX stays small (status only).
    esp_err_t err = uart_driver_install(cfg_.uart_num, 16384, 256, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_param_config(cfg_.uart_num, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(cfg_.uart_num, cfg_.pin_tx, cfg_.pin_rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "UartBridge init OK (TX=%d RX=%d baud=%d)",
             cfg_.pin_tx, cfg_.pin_rx, cfg_.baud_rate);
    return true;
}

void UartBridge::start()
{
    if (started_) return;
    started_ = true;

    // 4 KB: rxLoop now carries a bulk read buffer AND runs the per-chunk WS send
    // (image_chunk_cb_ -> sendBinaryWhole) synchronously on this stack.
    xTaskCreatePinnedToCore(&UartBridge::rxTaskEntry, "UartBridge", 4096, this, 3, &rx_task_, 0);
    ESP_LOGI(TAG, "UartBridge started");
}

void UartBridge::stop()
{
    if (!started_) return;
    started_ = false;
    vTaskDelay(pdMS_TO_TICKS(100));
    rx_task_ = nullptr;
    uart_driver_delete(cfg_.uart_num);
    ESP_LOGI(TAG, "UartBridge stopped");
}

bool UartBridge::sendFrame(uart_proto::MsgType type, const uint8_t* payload, uint8_t len)
{
    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t frame_len = uart_proto::buildFrame(frame, type, payload, len);
    if (frame_len == 0) return false;
    int written = uart_write_bytes(cfg_.uart_num, frame, frame_len);
    return written == (int)frame_len;
}

bool UartBridge::sendStatusUpdate(uint8_t interaction, uint8_t connectivity,
                                   uint8_t system_state, uint8_t emotion,
                                   uint8_t fail_reason)
{
    uart_proto::StatusPayload sp;
    sp.interaction   = interaction;
    sp.connection    = connectivity;
    sp.system_state  = system_state;
    sp.emotion       = emotion;
    sp.fail_reason   = fail_reason;
    return sendFrame(uart_proto::MsgType::STATUS_UPDATE, (const uint8_t*)&sp, sizeof(sp));
}

bool UartBridge::sendSyncData(const uint8_t* data, size_t len)
{
    if (len > uart_proto::MAX_PAYLOAD) return false;
    return sendFrame(uart_proto::MsgType::SYNC_DATA, data, (uint8_t)len);
}

bool UartBridge::sendOtaStatus(uint8_t state, uint8_t progress)
{
    uint8_t payload[2] = { state, progress };
    return sendFrame(uart_proto::MsgType::OTA_STATUS, payload, 2);
}

bool UartBridge::sendDeviceCmd(uart_proto::ControlCmd cmd,
                                const uint8_t* data, size_t len)
{
    if (1 + len > uart_proto::MAX_PAYLOAD) return false;
    uint8_t payload[uart_proto::MAX_PAYLOAD];
    payload[0] = (uint8_t)cmd;
    if (data && len > 0) {
        for (size_t i = 0; i < len; ++i) payload[1 + i] = data[i];
    }
    return sendFrame(uart_proto::MsgType::DEVICE_CMD, payload, (uint8_t)(1 + len));
}

void UartBridge::rxTaskEntry(void* arg)
{
    static_cast<UartBridge*>(arg)->rxLoop();
}

void UartBridge::rxLoop()
{
    ESP_LOGI(TAG, "RX task started");

    uart_proto::FrameParser parser;
    // Bulk read: pulling one byte per uart_read_bytes call (~46k calls/s at
    // 460800 baud) couldn't keep the RX FIFO drained during an image burst.
    // Drain up to a chunk at a time, then feed the parser byte-by-byte.
    uint8_t buf[256];

    while (started_) {
        int n = uart_read_bytes(cfg_.uart_num, buf, sizeof(buf), pdMS_TO_TICKS(20));
        if (n <= 0) continue;

        for (int b = 0; b < n; ++b) {
            if (!parser.feed(buf[b])) continue;

            uint8_t type = parser.getType();
            const uint8_t* payload = parser.getPayload();
            uint8_t payload_len = parser.getPayloadLen();

            if (type == (uint8_t)uart_proto::MsgType::CONTROL_CMD && payload_len >= 1) {
                auto cmd = static_cast<uart_proto::ControlCmd>(payload[0]);
                ESP_LOGI(TAG, "RX CONTROL_CMD: cmd=0x%02X len=%d", payload[0], payload_len);
                if (control_cb_) {
                    control_cb_(cmd, &payload[1], payload_len - 1);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::LOG_ENTRY && payload_len > 0) {
                if (log_cb_) {
                    log_cb_(payload, payload_len);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::IMAGE_CHUNK &&
                       payload_len >= uart_proto::image::CHUNK_HDR) {
                handleImageChunk(payload, payload_len);
            }
        }
    }

    ESP_LOGW(TAG, "RX task ended");
    resetImage();
    vTaskDelete(nullptr);
}

// Stream a JPEG from IMAGE_CHUNK frames. Default path: forward each chunk on
// (image_chunk_cb_) so the C5 never buffers the whole image. Legacy fallback:
// reassemble into img_buf_ and fire image_cb_ on the last chunk.
void UartBridge::handleImageChunk(const uint8_t* p, uint8_t len)
{
    if (image_chunk_cb_) {
        image_chunk_cb_(p, len);
        return;
    }

    namespace im = uart_proto::image;
    uint16_t seq   = (uint16_t)(p[0] | (p[1] << 8));
    uint8_t  flags = p[2];
    const uint8_t* data = p + im::CHUNK_HDR;
    size_t data_len = len - im::CHUNK_HDR;

    if (flags & im::FLAG_FIRST) {
        resetImage();
        if (data_len < im::FIRST_HDR) return;
        uint32_t total = (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
                         ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
        if (total == 0 || total > 64 * 1024) {
            ESP_LOGW(TAG, "image too big/zero: %lu", (unsigned long)total);
            return;
        }
        img_buf_ = (uint8_t*)malloc(total);
        if (!img_buf_) {
            ESP_LOGE(TAG, "image malloc %lu failed", (unsigned long)total);
            return;
        }
        img_total_ = total;
        img_recv_  = 0;
        img_seq_   = 0;
        data     += im::FIRST_HDR;
        data_len -= im::FIRST_HDR;
    }

    if (!img_buf_) return;                 // a non-first chunk with no buffer
    if (seq != img_seq_) {                 // lost/out-of-order chunk → abort frame
        ESP_LOGW(TAG, "image seq gap %u != %u, drop frame", seq, img_seq_);
        resetImage();
        return;
    }
    if (img_recv_ + data_len > img_total_) { resetImage(); return; }

    memcpy(img_buf_ + img_recv_, data, data_len);
    img_recv_ += data_len;
    img_seq_++;

    if (flags & im::FLAG_LAST) {
        if (img_recv_ == img_total_ && image_cb_) {
            ESP_LOGI(TAG, "image complete: %zu bytes", img_total_);
            image_cb_(img_buf_, img_total_);
        }
        resetImage();
    }
}

void UartBridge::resetImage()
{
    if (img_buf_) { free(img_buf_); img_buf_ = nullptr; }
    img_total_ = 0;
    img_recv_  = 0;
    img_seq_   = 0;
}
