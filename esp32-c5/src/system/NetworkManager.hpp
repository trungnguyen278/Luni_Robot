#pragma once
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include <atomic>
#include <cstring>

#include "WifiService.hpp"   // WifiInfo

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
    // Send a complete binary payload (e.g. a camera JPEG) as one WS frame,
    // bypassing the audio TX batching. See WebSocketClient::sendBinaryWhole.
    esp_err_t sendBinaryWhole(const uint8_t* data, size_t len);
    void waitTxDrain(uint32_t timeout_ms = 500);
    size_t getWsTxFreeSpace() const;

    // State updates forwarded to server
    void sendStateUpdate(const state::StateEvent& event);
    // Emotion update with the full 45-key string (not the lossy numeric enum).
    // Sent explicitly by the emotion handler; the generic sendStateUpdate path
    // skips EMOTION so the app/web only ever see the string form.
    void sendEmotionUpdate(const char* key);
    // OTA download/flash progress → server (percent + phase). Lets the cloud
    // advance OTAHistory and show progress in the app.
    void sendOtaProgress(uint8_t percent, const char* phase);
    void sendAudioUplink(const uint8_t* opus_data, size_t len);

    // Speaking session tracking
    bool isSpeakingSessionActive() const { return speaking_session_active_; }
    void startSpeakingSession()          { speaking_session_active_ = true; }
    void endSpeakingSession()            { speaking_session_active_ = false; }

    // OTA
    void checkOtaUpdate();

    // Latest WiFi scan captured just before entering BLE provisioning, served
    // to the app over BLE so it always sees a fresh, real network list.
    const std::vector<WifiInfo>& scannedNetworks() const { return last_scan_; }

    // Access sub-managers
    WebSocketClient* getWsClient() { return ws_.get(); }
    OtaManager* getOtaManager() { return ota_.get(); }
    DataSyncManager* getDataSync() { return sync_.get(); }
    UartBridge* getUartBridge() { return uart_bridge_; }

private:
    void setupWifi();
    void setupWebSocket();
    void sendDeviceInfo();

    // Connection state machine task
    static void connectionTaskEntry(void* arg);
    void runConnectionStateMachine();
    void transition(state::ConnectionState to,
                    state::ConnectFailReason reason = state::ConnectFailReason::NONE);

    // Scan WiFi and cache into last_scan_. Call right before transitioning into
    // BLE_PROVISIONING so the app gets the freshest list.
    void scanForProvisioning();

    // Map the WifiService's last disconnect reason to a ConnectFailReason so the
    // retry policy table (wrong-password vs AP-not-found vs DHCP) is actually
    // used instead of every WiFi failure collapsing to WIFI_TIMEOUT.
    state::ConnectFailReason classifyWifiFailure() const;

    // Heartbeat
    static void heartbeatTimerCb(void* arg);
    void startHeartbeat();
    void stopHeartbeat();
    void sendHeartbeat();

    // Interaction watchdog: keeps a voice turn from getting stuck if the
    // server never answers or the TTS stream stalls (see checkInteraction).
    static void interactionWatchdogCb(void* arg);
    void checkInteraction();

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
    bool device_token_exists_ = false;
    bool was_online_ = false;
    bool awaiting_provision_ = false;
    std::vector<WifiInfo> last_scan_;

    // Timers
    esp_timer_handle_t heartbeat_timer_ = nullptr;
    esp_timer_handle_t log_flush_timer_ = nullptr;
    esp_timer_handle_t interaction_wd_timer_ = nullptr;

    // Interaction watchdog bookkeeping (ms since boot, esp_timer clock)
    std::atomic<uint32_t> inter_state_since_ms_{0};
    std::atomic<uint32_t> last_downlink_ms_{0};
    int inter_wd_sub_id_ = -1;

    TaskHandle_t conn_task_ = nullptr;

    std::atomic<bool> started_{false};
    std::atomic<bool> ws_connected_{false};
    std::atomic<bool> ws_auth_failed_{false};
    std::atomic<bool> speaking_session_active_{false};
    // Sticky per-message drop flag for the audio downlink: once any fragment
    // of a WS binary message is dropped, the rest of that message must be
    // dropped too or the [2B len][opus] stream loses alignment.
    bool dl_msg_dropping_ = false;
};
