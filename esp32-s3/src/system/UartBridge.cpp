#include "UartBridge.hpp"
#include "esp_log.h"

static const char* TAG = "UartBridge";

// Delay between image chunks (ms). Matches the C5's ~1 chunk/RTT WS-forward rate
// so its 16 KB UART ring never overflows. See sendImage() for the full rationale.
static constexpr uint32_t CHUNK_PACING_MS = 100;

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

    esp_err_t err = uart_driver_install(cfg_.uart_num, 1024, 256, 0, nullptr, 0);
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

    // 8 KB, not 3 KB: rxLoop dispatches status/device callbacks synchronously,
    // and those run the full state fan-out on THIS stack — StateManager notify →
    // DisplayManager.showEmotion (pickVariant + ESP_LOGI) + AudioManager handlers.
    // That chain (esp. nested ESP_LOGI vprintf) overflowed a 3 KB stack on
    // SET_EMOTION. Keep heavy work off this task if it grows further.
    xTaskCreatePinnedToCore(&UartBridge::rxTaskEntry, "UartBridge", 8192, this, 3, &rx_task_, 0);
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

bool UartBridge::sendControlCmd(uart_proto::ControlCmd cmd,
                                 const uint8_t* data, size_t len)
{
    if (1 + len > uart_proto::MAX_PAYLOAD) return false;

    uint8_t payload[uart_proto::MAX_PAYLOAD];
    payload[0] = (uint8_t)cmd;
    if (data && len > 0) {
        for (size_t i = 0; i < len; ++i) payload[1 + i] = data[i];
    }

    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t frame_len = uart_proto::buildFrame(frame, uart_proto::MsgType::CONTROL_CMD,
                                               payload, (uint8_t)(1 + len));
    if (frame_len == 0) return false;

    int written = uart_write_bytes(cfg_.uart_num, frame, frame_len);
    return written == (int)frame_len;
}

bool UartBridge::sendLogEntry(const uint8_t* data, size_t len)
{
    if (len > uart_proto::MAX_PAYLOAD) return false;

    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t frame_len = uart_proto::buildFrame(frame, uart_proto::MsgType::LOG_ENTRY,
                                               data, (uint8_t)len);
    if (frame_len == 0) return false;

    int written = uart_write_bytes(cfg_.uart_num, frame, frame_len);
    return written == (int)frame_len;
}

bool UartBridge::sendImage(const uint8_t* jpeg, size_t len, uint16_t width, uint16_t height)
{
    namespace up = uart_proto;
    if (!jpeg || len == 0) return false;

    uint8_t payload[up::MAX_PAYLOAD];
    uint8_t frame[up::MAX_FRAME_SIZE];
    uint16_t seq = 0;
    size_t off = 0;
    bool first = true;

    while (off < len) {
        size_t p = 0;
        payload[p++] = (uint8_t)(seq & 0xFF);
        payload[p++] = (uint8_t)(seq >> 8);
        size_t flags_idx = p++;            // filled in once we know FIRST/LAST
        uint8_t flags = 0;

        if (first) {
            flags |= up::image::FLAG_FIRST;
            payload[p++] = (uint8_t)(len & 0xFF);
            payload[p++] = (uint8_t)((len >> 8) & 0xFF);
            payload[p++] = (uint8_t)((len >> 16) & 0xFF);
            payload[p++] = (uint8_t)((len >> 24) & 0xFF);
            payload[p++] = (uint8_t)(width & 0xFF);
            payload[p++] = (uint8_t)(width >> 8);
            payload[p++] = (uint8_t)(height & 0xFF);
            payload[p++] = (uint8_t)(height >> 8);
            first = false;
        }

        size_t room = up::MAX_PAYLOAD - p;
        size_t n = (len - off < room) ? (len - off) : room;
        for (size_t i = 0; i < n; ++i) payload[p++] = jpeg[off + i];
        off += n;

        if (off >= len) flags |= up::image::FLAG_LAST;
        payload[flags_idx] = flags;

        size_t frame_len = up::buildFrame(frame, up::MsgType::IMAGE_CHUNK,
                                          payload, (uint8_t)p);
        if (frame_len == 0) return false;
        if (uart_write_bytes(cfg_.uart_num, frame, frame_len) != (int)frame_len)
            return false;

        seq++;
        // Pace the burst to the rate the C5 can actually forward each chunk over
        // WS (~1 chunk per network RTT; ~100ms over the Cloudflare tunnel). The C5
        // has no heap to buffer a whole image and its 16 KB UART ring overflowed
        // when we dumped all ~112 chunks at line rate, losing the tail (incl. the
        // FLAG_LAST chunk) so the frame never completed. The JPEG lives here in S3
        // PSRAM, so we throttle the producer instead. No real time cost: the WS
        // link is the bottleneck (~2.5 KB/s) regardless. TODO: drop once the C5
        // WS send is de-Nagled / batched and can keep up at line rate.
        vTaskDelay(pdMS_TO_TICKS(CHUNK_PACING_MS));
    }
    ESP_LOGI(TAG, "Image sent: %zu bytes in %u chunks", len, (unsigned)seq);
    return true;
}

void UartBridge::rxTaskEntry(void* arg)
{
    static_cast<UartBridge*>(arg)->rxLoop();
}

void UartBridge::rxLoop()
{
    ESP_LOGI(TAG, "RX task started");

    uart_proto::FrameParser parser;
    uint8_t byte;

    while (started_) {
        int len = uart_read_bytes(cfg_.uart_num, &byte, 1, pdMS_TO_TICKS(50));
        if (len <= 0) continue;

        if (parser.feed(byte)) {
            uint8_t type = parser.getType();
            const uint8_t* payload = parser.getPayload();
            uint8_t payload_len = parser.getPayloadLen();

            if (type == (uint8_t)uart_proto::MsgType::STATUS_UPDATE &&
                payload_len >= sizeof(uart_proto::StatusPayload)) {
                auto* sp = reinterpret_cast<const uart_proto::StatusPayload*>(payload);
                if (status_cb_) {
                    status_cb_(sp->interaction, sp->connection,
                               sp->system_state, sp->emotion);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::CONTROL_CMD &&
                       payload_len >= 1) {
                auto cmd = static_cast<uart_proto::ControlCmd>(payload[0]);
                if (ctrl_cb_) {
                    ctrl_cb_(cmd, payload + 1, payload_len - 1);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::SYNC_DATA &&
                       payload_len > 0) {
                if (sync_cb_) {
                    sync_cb_(payload, payload_len);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::OTA_STATUS &&
                       payload_len >= 2) {
                if (ota_cb_) {
                    ota_cb_(payload[0], payload[1]);
                }
            } else if (type == (uint8_t)uart_proto::MsgType::DEVICE_CMD &&
                       payload_len >= 1) {
                auto cmd = static_cast<uart_proto::ControlCmd>(payload[0]);
                if (devcmd_cb_) {
                    devcmd_cb_(cmd, payload + 1, payload_len - 1);
                }
            }
        }
    }

    ESP_LOGW(TAG, "RX task ended");
    vTaskDelete(nullptr);
}
