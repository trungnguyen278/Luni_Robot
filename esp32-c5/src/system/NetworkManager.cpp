#include "NetworkManager.hpp"
#include "WifiService.hpp"
#include "WebSocketClient.hpp"
#include "MqttClient.hpp"
#include "AudioManager.hpp"
#include "Version.hpp"

#include "esp_log.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "NetworkManager";

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager() { stop(); }

bool NetworkManager::init(const Config& cfg)
{
    cfg_ = cfg;
    ESP_LOGI(TAG, "init: ws=%s, mqtt=%s", cfg_.ws_url.c_str(), cfg_.mqtt_url.c_str());

    wifi_ = std::make_unique<WifiService>();
    ws_   = std::make_unique<WebSocketClient>();
    mqtt_ = std::make_unique<MqttClient>();

    setupWifi();
    setupWebSocket();
    setupMqtt();

    return true;
}

void NetworkManager::start()
{
    if (started_) return;
    started_ = true;

    // Start WiFi connection
    if (!cfg_.wifi_ssid.empty()) {
        wifi_->setCredentials(cfg_.wifi_ssid, cfg_.wifi_pass);
    }
    wifi_->startSTA();

    ESP_LOGI(TAG, "NetworkManager started");
}

void NetworkManager::stop()
{
    if (!started_) return;
    started_ = false;

    if (ws_) ws_->disconnect();
    if (mqtt_) mqtt_->disconnect();

    vTaskDelay(pdMS_TO_TICKS(200));
    uplink_task_ = nullptr;

    ESP_LOGI(TAG, "NetworkManager stopped");
}

void NetworkManager::setCredentials(const std::string& ssid, const std::string& pass)
{
    cfg_.wifi_ssid = ssid;
    cfg_.wifi_pass = pass;
    if (wifi_) wifi_->setCredentials(ssid, pass);
}

void NetworkManager::setMicBuffer(StreamBufferHandle_t mic_buf) { mic_buf_ = mic_buf; }
void NetworkManager::setAudioManager(AudioManager* mgr) { audio_mgr_ = mgr; }

// === WiFi setup ===

void NetworkManager::setupWifi()
{
    wifi_->onStatusChanged([this](WifiService::Status status) {
        switch (status) {
        case WifiService::Status::CONNECTED:
            ESP_LOGI(TAG, "WiFi connected");
            StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);

            // Connect WebSocket
            if (ws_ && !cfg_.ws_url.empty()) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::CONNECTING_WS);
                ws_->connect(cfg_.ws_url);
            }

            // Connect MQTT
            if (mqtt_ && !cfg_.mqtt_url.empty()) {
                MqttClient::Config mqtt_cfg;
                mqtt_cfg.uri = cfg_.mqtt_url;
                mqtt_cfg.username = cfg_.user_id.empty() ? std::string(getDeviceEfuseID()) : cfg_.user_id;
                mqtt_cfg.password = cfg_.tx_key;
                mqtt_->connect(mqtt_cfg);
            }
            break;

        case WifiService::Status::DISCONNECTED:
            ESP_LOGW(TAG, "WiFi disconnected");
            if (!ws_immune_) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::OFFLINE);
                ws_connected_ = false;
            }
            break;

        case WifiService::Status::CONNECTING:
            StateManager::instance().setConnectivityState(state::ConnectivityState::CONNECTING_WIFI);
            break;
        }
    });
}

// === WebSocket setup ===

void NetworkManager::setupWebSocket()
{
    ws_->onStatusChanged([this](WebSocketClient::Status status) {
        switch (status) {
        case WebSocketClient::Status::CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            ws_connected_ = true;
            StateManager::instance().setConnectivityState(state::ConnectivityState::ONLINE);
            sendDeviceHandshake();

            // Start uplink task
            if (!uplink_task_ && mic_buf_) {
                xTaskCreatePinnedToCore(&NetworkManager::uplinkTaskEntry,
                    "Uplink", 4096, this, 4, &uplink_task_, 0);
            }
            break;

        case WebSocketClient::Status::DISCONNECTED:
            ESP_LOGW(TAG, "WebSocket disconnected");
            ws_connected_ = false;
            if (!ws_immune_) {
                StateManager::instance().setConnectivityState(state::ConnectivityState::WIFI_CONNECTED);
            }
            if (disconnect_cb_) disconnect_cb_();
            break;

        case WebSocketClient::Status::CONNECTING:
            break;
        }
    });

    ws_->onTextMessage([this](const std::string& msg) {
        ESP_LOGI(TAG, "WS text: %s", msg.c_str());

        // Check for emotion code prefix (2-digit code before command)
        if (msg.length() >= 2) {
            std::string potential_code = msg.substr(0, 2);
            if (potential_code[0] >= '0' && potential_code[0] <= '9' &&
                potential_code[1] >= '0' && potential_code[1] <= '9') {
                auto emotion = parseEmotionCode(potential_code);
                StateManager::instance().setEmotionState(emotion);
                if (msg.length() > 2) {
                    if (text_cb_) text_cb_(msg.substr(2));
                    return;
                }
            }
        }

        if (text_cb_) text_cb_(msg);
    });

    ws_->onBinaryMessage([this](const uint8_t* data, size_t len) {
        if (binary_cb_) binary_cb_(data, len);
    });
}

// === MQTT setup ===

void NetworkManager::setupMqtt()
{
    mqtt_->onConnected([this]() {
        ESP_LOGI(TAG, "MQTT connected");
        std::string cmd_topic = "devices/" + std::string(getDeviceEfuseID()) + "/cmd";
        mqtt_->subscribe(cmd_topic);
    });

    mqtt_->onMessage([this](const std::string& topic, const std::string& payload) {
        ESP_LOGI(TAG, "MQTT: topic=%s", topic.c_str());

        cJSON* root = cJSON_Parse(payload.c_str());
        if (!root) return;

        cJSON* cmd_item = cJSON_GetObjectItem(root, "cmd");
        if (cmd_item && cJSON_IsString(cmd_item)) {
            std::string cmd = cmd_item->valuestring;

            if (cmd == "set_volume") {
                cJSON* vol = cJSON_GetObjectItem(root, "volume");
                if (vol && cJSON_IsNumber(vol) && audio_mgr_) {
                    audio_mgr_->setVolume((uint8_t)vol->valueint);
                }
            } else if (cmd == "reboot") {
                esp_restart();
            } else if (cmd == "request_status") {
                std::string status_topic = "devices/" + std::string(getDeviceEfuseID()) + "/status";
                auto& sm = StateManager::instance();
                char buf[256];
                snprintf(buf, sizeof(buf),
                    "{\"status\":\"ok\",\"device_id\":\"%s\",\"firmware_version\":\"%s\","
                    "\"connectivity\":%d,\"interaction\":%d}",
                    getDeviceEfuseID(), app_meta::APP_VERSION,
                    (int)sm.getConnectivityState(), (int)sm.getInteractionState());
                mqtt_->publish(status_topic, buf);
            }
        }
        cJSON_Delete(root);
    });
}

// === WebSocket operations ===

void NetworkManager::sendText(const std::string& msg)
{
    if (ws_ && ws_connected_) ws_->sendText(msg);
}

void NetworkManager::sendBinary(const uint8_t* data, size_t len)
{
    if (ws_ && ws_connected_) ws_->sendBinary(data, len);
}

void NetworkManager::setWSImmuneMode(bool enable) { ws_immune_ = enable; }

void NetworkManager::sendDeviceHandshake()
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"cmd\":\"device_handshake\",\"device_id\":\"%s\","
        "\"firmware_version\":\"%s\",\"device_name\":\"%s\"}",
        getDeviceEfuseID(), app_meta::APP_VERSION, app_meta::DEVICE_MODEL);
    sendText(buf);
}

// === Emotion parsing ===

state::EmotionState NetworkManager::parseEmotionCode(const std::string& code)
{
    if (code == "01") return state::EmotionState::HAPPY;
    if (code == "11") return state::EmotionState::SAD;
    if (code == "02") return state::EmotionState::ANGRY;
    if (code == "12") return state::EmotionState::CONFUSED;
    if (code == "03") return state::EmotionState::EXCITED;
    if (code == "13") return state::EmotionState::CALM;
    return state::EmotionState::NEUTRAL;
}

// === Uplink task ===

void NetworkManager::uplinkTaskEntry(void* arg)
{
    static_cast<NetworkManager*>(arg)->uplinkLoop();
}

void NetworkManager::uplinkLoop()
{
    ESP_LOGI(TAG, "Uplink task started");
    uint8_t buf[512];

    while (started_) {
        if (!ws_connected_ ||
            StateManager::instance().getInteractionState() != state::InteractionState::LISTENING) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t got = xStreamBufferReceive(mic_buf_, buf, sizeof(buf), pdMS_TO_TICKS(20));
        if (got > 0) {
            sendBinary(buf, got);
        }
    }

    ESP_LOGW(TAG, "Uplink task ended");
    vTaskDelete(nullptr);
}
