#pragma once
#include <memory>
#include <functional>
#include <string>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"

#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

class WifiService;
class WebSocketClient;
class MqttClient;
class AudioManager;

// NetworkManager for ESP32-C5: WiFi + WebSocket + MQTT
// No BLE, no OTA, no display (those are on S3).
class NetworkManager {
public:
    struct Config {
        std::string wifi_ssid;
        std::string wifi_pass;
        std::string ws_url;      // e.g. "ws://host:port/ws"
        std::string mqtt_url;    // e.g. "mqtt://host:port"
        std::string user_id;     // For MQTT topic namespace
        std::string tx_key;      // MQTT password
    };

    using BinaryCb = std::function<void(const uint8_t* data, size_t len)>;
    using TextCb   = std::function<void(const std::string& msg)>;
    using VoidCb   = std::function<void()>;

    NetworkManager();
    ~NetworkManager();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Set WiFi credentials (can be called before or after init)
    void setCredentials(const std::string& ssid, const std::string& pass);

    // Audio buffer wiring
    void setMicBuffer(StreamBufferHandle_t mic_encoded_buf);
    void setAudioManager(AudioManager* mgr);

    // Server message callbacks
    void onServerBinary(BinaryCb cb)   { binary_cb_ = std::move(cb); }
    void onServerText(TextCb cb)       { text_cb_ = std::move(cb); }
    void onDisconnect(VoidCb cb)       { disconnect_cb_ = std::move(cb); }

    // WebSocket control
    void sendText(const std::string& msg);
    void sendBinary(const uint8_t* data, size_t len);
    void setWSImmuneMode(bool enable);

    // Speaking session tracking (for audio gating)
    bool isSpeakingSessionActive() const { return speaking_session_active_; }
    void startSpeakingSession()          { speaking_session_active_ = true; }
    void endSpeakingSession()            { speaking_session_active_ = false; }

    // Emotion parsing
    static state::EmotionState parseEmotionCode(const std::string& code);

private:
    void setupWifi();
    void setupWebSocket();
    void setupMqtt();
    void sendDeviceHandshake();

    // Uplink task: reads mic buffer and sends to server
    static void uplinkTaskEntry(void* arg);
    void uplinkLoop();

    Config cfg_;

    std::unique_ptr<WifiService>     wifi_;
    std::unique_ptr<WebSocketClient> ws_;
    std::unique_ptr<MqttClient>      mqtt_;
    AudioManager* audio_mgr_ = nullptr;

    StreamBufferHandle_t mic_buf_ = nullptr;
    TaskHandle_t uplink_task_ = nullptr;

    std::atomic<bool> started_{false};
    std::atomic<bool> ws_connected_{false};
    std::atomic<bool> ws_immune_{false};
    std::atomic<bool> speaking_session_active_{false};

    BinaryCb binary_cb_;
    TextCb   text_cb_;
    VoidCb   disconnect_cb_;
};
