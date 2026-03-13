#include "UartBridge.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "UartBridge";

UartBridge::~UartBridge()
{
    stop();
}

bool UartBridge::init(const Config& cfg)
{
    cfg_ = cfg;
    uart_port_ = (uart_port_t)cfg_.uart_num;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = cfg_.baud_rate;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(uart_port_, cfg_.rx_buf_size, cfg_.tx_buf_size, 0, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_param_config(uart_port_, &uart_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(err));
        return false;
    }

    err = uart_set_pin(uart_port_, cfg_.tx_pin, cfg_.rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "UART set pin failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "UART%d initialized (TX=%d, RX=%d, %d baud)",
             uart_port_, cfg_.tx_pin, cfg_.rx_pin, cfg_.baud_rate);
    return true;
}

void UartBridge::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(
        &UartBridge::rxTaskEntry,
        "UartRxTask",
        4096,
        this,
        3,
        &rx_task_,
        0
    );
}

void UartBridge::stop()
{
    if (!started_) return;
    started_ = false;

    if (rx_task_) {
        vTaskDelay(pdMS_TO_TICKS(200));
        rx_task_ = nullptr;
    }
}

// --- TX: Send frames TO C5 ---

bool UartBridge::sendFrame(uint8_t type, const uint8_t* payload, uint16_t len)
{
    uint8_t frame[uart_proto::FRAME_OVERHEAD + uart_proto::MAX_PAYLOAD];
    size_t frame_len = uart_proto::buildFrame(frame, sizeof(frame), type, payload, len);
    if (frame_len == 0) return false;

    int written = uart_write_bytes(uart_port_, frame, frame_len);
    return written == (int)frame_len;
}

bool UartBridge::sendWifiConfig(const std::string& ssid, const std::string& pass)
{
    uint8_t payload[uart_proto::MAX_PAYLOAD];
    size_t pos = 0;
    payload[pos++] = (uint8_t)ssid.size();
    memcpy(&payload[pos], ssid.data(), ssid.size()); pos += ssid.size();
    payload[pos++] = (uint8_t)pass.size();
    memcpy(&payload[pos], pass.data(), pass.size()); pos += pass.size();
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgFromS3::WIFI_CONFIG), payload, (uint16_t)pos);
}

bool UartBridge::sendMqttConfig(const std::string& uri, const std::string& user, const std::string& pass)
{
    uint8_t payload[uart_proto::MAX_PAYLOAD];
    size_t pos = 0;
    auto writeStr = [&](const std::string& s) {
        payload[pos++] = (uint8_t)s.size();
        memcpy(&payload[pos], s.data(), s.size()); pos += s.size();
    };
    writeStr(uri);
    writeStr(user);
    writeStr(pass);
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgFromS3::MQTT_CONFIG), payload, (uint16_t)pos);
}

bool UartBridge::sendWsConfig(const std::string& url)
{
    uint8_t payload[uart_proto::MAX_PAYLOAD];
    size_t pos = 0;
    payload[pos++] = (uint8_t)url.size();
    memcpy(&payload[pos], url.data(), url.size()); pos += url.size();
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgFromS3::WS_CONFIG), payload, (uint16_t)pos);
}

bool UartBridge::sendAdcData(uint16_t voltage_mv, uint8_t battery_pct, bool charging)
{
    uint8_t payload[4];
    payload[0] = (uint8_t)(voltage_mv & 0xFF);
    payload[1] = (uint8_t)((voltage_mv >> 8) & 0xFF);
    payload[2] = battery_pct;
    payload[3] = charging ? 1 : 0;
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgFromS3::ADC_DATA), payload, 4);
}

bool UartBridge::sendCommand(uart_proto::Command cmd, const uint8_t* data, size_t data_len)
{
    uint8_t payload[uart_proto::MAX_PAYLOAD];
    payload[0] = static_cast<uint8_t>(cmd);
    if (data && data_len > 0) {
        memcpy(&payload[1], data, data_len);
    }
    return sendFrame(static_cast<uint8_t>(uart_proto::MsgFromS3::COMMAND), payload, (uint16_t)(1 + data_len));
}

// --- RX: Receive frames FROM C5 ---

void UartBridge::rxTaskEntry(void* arg)
{
    static_cast<UartBridge*>(arg)->rxTaskLoop();
}

void UartBridge::rxTaskLoop()
{
    ESP_LOGI(TAG, "UART RX task started");

    uart_proto::FrameParser parser;
    uint8_t byte;

    while (started_) {
        int len = uart_read_bytes(uart_port_, &byte, 1, pdMS_TO_TICKS(50));
        if (len <= 0) continue;

        auto result = parser.feed(byte);
        switch (result) {
        case uart_proto::FrameParser::Result::FRAME_READY:
            handleFrame(parser.msgType(), parser.payload(), parser.payloadLen());
            parser.reset();
            break;
        case uart_proto::FrameParser::Result::ERROR:
            ESP_LOGW(TAG, "Frame parse error, resetting");
            parser.reset();
            break;
        case uart_proto::FrameParser::Result::NEED_MORE:
            break;
        }
    }

    ESP_LOGW(TAG, "UART RX task ended");
    vTaskDelete(nullptr);
}

void UartBridge::handleFrame(uint8_t type, const uint8_t* payload, uint16_t len)
{
    auto msg = static_cast<uart_proto::MsgToS3>(type);

    switch (msg) {
    case uart_proto::MsgToS3::STATUS_UPDATE: {
        if (len < 3) break;
        uint8_t interaction = payload[0];
        uint8_t connectivity = payload[1];
        uint8_t system_state = payload[2];
        ESP_LOGD(TAG, "Status update: inter=%d conn=%d sys=%d", interaction, connectivity, system_state);
        if (status_cb_) status_cb_(interaction, connectivity, system_state);
        break;
    }
    case uart_proto::MsgToS3::DISPLAY_CMD: {
        if (len < 1) break;
        uint8_t emotion = payload[0];
        ESP_LOGD(TAG, "Display cmd: emotion=%d", emotion);
        if (display_cb_) display_cb_(emotion);
        break;
    }
    case uart_proto::MsgToS3::CONFIG_REQ: {
        ESP_LOGI(TAG, "Config request from C5");
        if (config_req_cb_) config_req_cb_();
        break;
    }
    case uart_proto::MsgToS3::ACK:
        ESP_LOGD(TAG, "ACK received from C5");
        break;
    default:
        ESP_LOGW(TAG, "Unknown message type: 0x%02X", type);
        break;
    }
}
