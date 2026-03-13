#pragma once
#include <functional>
#include <atomic>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "UartProtocol.hpp"

// UartBridge: UART communication between ESP32-S3 and ESP32-C5
// S3 sends: WiFi config, MQTT config, WS config, ADC data, commands TO C5
// S3 receives: status updates, display commands (emotion) FROM C5
class UartBridge {
public:
    struct Config {
        int uart_num    = 1;
        int tx_pin      = 17;
        int rx_pin      = 18;
        int baud_rate   = 115200;
        int rx_buf_size = 1024;
        int tx_buf_size = 512;
    };

    // Callbacks for data received FROM C5
    using StatusUpdateCb = std::function<void(uint8_t interaction, uint8_t connectivity, uint8_t system_state)>;
    using DisplayCmdCb   = std::function<void(uint8_t emotion)>;
    using ConfigReqCb    = std::function<void()>;

    UartBridge() = default;
    ~UartBridge();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Register callbacks for incoming messages from C5
    void onStatusUpdate(StatusUpdateCb cb) { status_cb_ = std::move(cb); }
    void onDisplayCmd(DisplayCmdCb cb)     { display_cb_ = std::move(cb); }
    void onConfigRequest(ConfigReqCb cb)   { config_req_cb_ = std::move(cb); }

    // Send messages TO C5
    bool sendWifiConfig(const std::string& ssid, const std::string& pass);
    bool sendMqttConfig(const std::string& uri, const std::string& user, const std::string& pass);
    bool sendWsConfig(const std::string& url);
    bool sendAdcData(uint16_t voltage_mv, uint8_t battery_pct, bool charging);
    bool sendCommand(uart_proto::Command cmd, const uint8_t* data = nullptr, size_t data_len = 0);

private:
    static void rxTaskEntry(void* arg);
    void rxTaskLoop();

    void handleFrame(uint8_t type, const uint8_t* payload, uint16_t len);
    bool sendFrame(uint8_t type, const uint8_t* payload, uint16_t len);

    Config cfg_{};
    uart_port_t uart_port_ = (uart_port_t)-1;
    std::atomic<bool> started_{false};
    TaskHandle_t rx_task_ = nullptr;

    // Callbacks
    StatusUpdateCb status_cb_;
    DisplayCmdCb   display_cb_;
    ConfigReqCb    config_req_cb_;
};
