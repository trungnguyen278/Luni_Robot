#pragma once
#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

class WifiService;
class WebSocketClient;
class SpiBridge;
class UartBridge;
class OtaManager;
class DataSyncManager;
class LogManager;

class NetworkManager {
public:
    struct Config {
        std::string wifi_ssid;
        std::string wifi_pass;
        std::string ws_url;
        std::string device_name;
        std::string device_token;
    };

    NetworkManager();
    ~NetworkManager();

    bool init(const Config& cfg);
    void start();
    void stop();

    void setCredentials(const std::string& ssid, const std::string& pass);

    // Bridge wiring
    void setSpiBridge(SpiBridge* bridge) { spi_bridge_ = bridge; }
    void setUartBridge(UartBridge* bridge) { uart_bridge_ = bridge; }

    // WebSocket operations
    void sendText(const char* json, size_t len);
    void sendText(const char* json) { sendText(json, strlen(json)); }
    void sendBinary(const uint8_t* data, size_t len);
    void waitTxDrain(uint32_t timeout_ms = 500);
    size_t getWsTxFreeSpace() const;

    // State updates forwarded to server
    void sendStateUpdate(const state::StateEvent& event);
    void sendAudioUplink(const uint8_t* opus_data, size_t len);

    // Speaking session tracking
    bool isSpeakingSessionActive() const { return speaking_session_active_; }
    void startSpeakingSession()          { speaking_session_active_ = true; }
    void endSpeakingSession()            { speaking_session_active_ = false; }

    // OTA
    void checkOtaUpdate();

    // Access sub-managers
    WebSocketClient* getWsClient() { return ws_.get(); }
    OtaManager* getOtaManager() { return ota_.get(); }
    DataSyncManager* getDataSync() { return sync_.get(); }

private:
    void setupWifi();
    void setupWebSocket();
    void sendDeviceInfo();

    // Connection state machine task
    static void connectionTaskEntry(void* arg);
    void runConnectionStateMachine();
    void transition(state::ConnectionState to,
                    state::ConnectFailReason reason = state::ConnectFailReason::NONE);

    // Heartbeat
    static void heartbeatTimerCb(void* arg);
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeat();

    // Log flush
    static void logFlushTimerCb(void* arg);

    Config cfg_;

    std::unique_ptr<WifiService>      wifi_;
    std::unique_ptr<WebSocketClient>  ws_;
    std::unique_ptr<OtaManager>       ota_;
    std::unique_ptr<DataSyncManager>  sync_;
    SpiBridge*  spi_bridge_  = nullptr;
    UartBridge* uart_bridge_ = nullptr;

    // Connection state
    state::ConnectionState conn_state_ = state::ConnectionState::OFFLINE;
    state::ConnectFailReason last_fail_ = state::ConnectFailReason::NONE;
    uint8_t reconnect_attempts_ = 0;
    bool wifi_credentials_exist_ = false;
    bool was_online_ = false;
    bool awaiting_provision_ = false;

    // Timers
    esp_timer_handle_t heartbeat_timer_ = nullptr;
    esp_timer_handle_t log_flush_timer_ = nullptr;

    TaskHandle_t conn_task_ = nullptr;

    std::atomic<bool> started_{false};
    std::atomic<bool> ws_connected_{false};
    std::atomic<bool> speaking_session_active_{false};
};
