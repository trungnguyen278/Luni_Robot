#include "NetworkManager.hpp"
#include "WifiService.hpp"
#include "WebSocketClient.hpp"
#include "system/OtaManager.hpp"
#include "system/DataSyncManager.hpp"
#include "system/LogManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"
#include "protocol/WsProtocol.hpp"
#include "protocol/WsMessageHandler.hpp"
#include "config/ServerConfig.hpp"
#include "Version.hpp"

#include "esp_log.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include <cstring>

static const char* TAG = "NetworkManager";

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager() { stop(); }

bool NetworkManager::init(const Config& cfg)
{
    cfg_ = cfg;
    wifi_credentials_exist_ = !cfg_.wifi_ssid.empty();

    ESP_LOGI(TAG, "init: ws=%s mac=%s",
             cfg_.ws_url.c_str(), getDLuniceEfuseID());

    wifi_ = std::make_unique<WifiService>();
    ws_   = std::make_unique<WebSocketClient>();
    ota_  = std::make_unique<OtaManager>();
    sync_ = std::make_unique<DataSyncManager>();

    wifi_->init();
    ws_->init();
    ws_->setDeviceInfo(getDLuniceEfuseID(), app_meta::APP_VERSION, app_meta::DLuniCE_MODEL);
    ota_->init(StateManager::instance());
    sync_->init();

    setupWifi();
    setupWebSocket();

    return true;
}

void NetworkManager::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(&NetworkManager::connectionTaskEntry,
                            "net_conn", 4096, this, 5, &conn_task_, 0);

    ESP_LOGI(TAG, "NetworkManager started");
}

void NetworkManager::stop()
{
    if (!started_) return;
    started_ = false;

    stopHeartbeat();

    if (log_flush_timer_) {
        esp_timer_stop(log_flush_timer_);
        esp_timer_delete(log_flush_timer_);
        log_flush_timer_ = nullptr;
    }

    if (ws_) ws_->disconnect();

    vTaskDelay(pdMS_TO_TICKS(200));
    conn_task_ = nullptr;
    ESP_LOGI(TAG, "NetworkManager stopped");
}

void NetworkManager::setCredentials(const std::string& ssid, const std::string& pass)
{
    cfg_.wifi_ssid = ssid;
    cfg_.wifi_pass = pass;
    wifi_credentials_exist_ = !ssid.empty();
    awaiting_provision_ = false;
}

// === WiFi setup ===

void NetworkManager::setupWifi()
{
    wifi_->onStatus([this](int status) {
        switch (status) {
        case 2: // GOT_IP
            ESP_LOGI(TAG, "WiFi connected");
            break;
        case 0: // DISCONNECTED
            ESP_LOGW(TAG, "WiFi disconnected");
            ws_connected_ = false;
            break;
        case 1: // CONNECTING
            break;
        }
    });
}

// === WebSocket setup ===

void NetworkManager::setupWebSocket()
{
    ws_->onStateChange([this](bool connected) {
        ws_connected_ = connected;
        if (!connected) {
            if (speaking_session_active_) {
                endSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
            }
        }
    });

    ws_->onAuthResult([this](bool success, int close_code) {
        if (success) {
            ESP_LOGI(TAG, "WS authenticated");
            sendDeviceInfo();
            startHeartbeat();

            // Start log flush timer
            if (!log_flush_timer_) {
                esp_timer_create_args_t args = {};
                args.callback = &NetworkManager::logFlushTimerCb;
                args.arg = this;
                args.name = "log_flush";
                esp_timer_create(&args, &log_flush_timer_);
                esp_timer_start_periodic(log_flush_timer_,
                    server_cfg::LOG_FLUSH_INTERVAL_MS * 1000);
            }
        }
    });

    // Incoming text messages — dispatch via WsMessageHandler
    ws_->onText([this](const char* data, size_t len) {
        ws::ParsedMessage msg = ws::parseMessage(data, len);
        if (!msg.valid) {
            ESP_LOGW(TAG, "Invalid WS message");
            return;
        }

        WsMessageHandler::handleMessage(msg, this);

        if (msg.root) cJSON_Delete(msg.root);
    });

    // Binary audio from server → relay to S3 via SPI
    ws_->onBinary([this](const uint8_t* data, size_t len) {
        if (!data || len == 0 || !spi_bridge_) return;

        if (!speaking_session_active_) {
            auto s = StateManager::instance().getInteractionState();
            if (s == state::InteractionState::PROCESSING ||
                s == state::InteractionState::SPEAKING) {
                startSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            } else {
                return;
            }
        }

        spi_bridge_->sendAudioDownlink(data, len);
    });
}

// === Send operations ===

void NetworkManager::sendText(const char* json, size_t len)
{
    if (ws_ && ws_connected_) ws_->sendText(json, len);
}

void NetworkManager::sendBinary(const uint8_t* data, size_t len)
{
    if (ws_ && ws_connected_) ws_->sendBinary(data, len);
}

void NetworkManager::waitTxDrain(uint32_t timeout_ms)
{
    if (ws_) ws_->waitTxDrain(timeout_ms);
}

size_t NetworkManager::getWsTxFreeSpace() const
{
    return ws_ ? ws_->getTxFreeSpace() : 0;
}

void NetworkManager::sendDeviceInfo()
{
    cJSON* msg = ws::createDeviceInfo(
        getDLuniceEfuseID(), app_meta::APP_VERSION,
        app_meta::DLuniCE_MODEL, cfg_.device_name.c_str());
    if (msg && ws_) {
        ws_->sendMessage(msg);
    }
}

void NetworkManager::sendStateUpdate(const state::StateEvent& event)
{
    const char* category = nullptr;
    switch (event.type) {
    case state::StateEvent::Type::CONNECTION:  category = "connectivity"; break;
    case state::StateEvent::Type::INTERACTION: category = "interaction"; break;
    case state::StateEvent::Type::OTA:         category = "ota"; break;
    case state::StateEvent::Type::SYSTEM:      category = "system"; break;
    case state::StateEvent::Type::EMOTION:     category = "emotion"; break;
    default: return;
    }

    cJSON* msg = ws::createStateUpdate(category, event.old_value, event.new_value);
    if (msg && ws_) {
        ws_->sendMessage(msg);
    }
}

void NetworkManager::sendAudioUplink(const uint8_t* opus_data, size_t len)
{
    sendBinary(opus_data, len);
}

void NetworkManager::checkOtaUpdate()
{
    if (ota_) ota_->checkUpdate(app_meta::APP_VERSION, app_meta::DLuniCE_MODEL);
}

// === Heartbeat ===

void NetworkManager::heartbeatTimerCb(void* arg)
{
    auto* self = static_cast<NetworkManager*>(arg);
    self->sendHeartbeat();
}

void NetworkManager::startHeartbeat()
{
    if (heartbeat_timer_) return;

    esp_timer_create_args_t args = {};
    args.callback = &NetworkManager::heartbeatTimerCb;
    args.arg = this;
    args.name = "heartbeat";
    esp_timer_create(&args, &heartbeat_timer_);
    esp_timer_start_periodic(heartbeat_timer_,
        server_cfg::HEARTBEAT_INTERVAL_MS * 1000);
}

void NetworkManager::stopHeartbeat()
{
    if (heartbeat_timer_) {
        esp_timer_stop(heartbeat_timer_);
        esp_timer_delete(heartbeat_timer_);
        heartbeat_timer_ = nullptr;
    }
}

void NetworkManager::sendHeartbeat()
{
    if (!ws_ || !ws_connected_) return;

    wifi_ap_record_t ap;
    int8_t rssi = 0;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        rssi = ap.rssi;
    }

    cJSON* msg = ws::createHeartbeat(
        (uint32_t)(esp_timer_get_time() / 1000000),
        (uint32_t)esp_get_free_heap_size(),
        rssi);
    if (msg) ws_->sendMessage(msg);
}

void NetworkManager::logFlushTimerCb(void* arg)
{
    auto* self = static_cast<NetworkManager*>(arg);
    if (self->ws_ && self->ws_connected_) {
        LogManager::instance().flush(*self->ws_);
    }
}

// === Connection state machine ===

void NetworkManager::transition(state::ConnectionState to, state::ConnectFailReason reason)
{
    conn_state_ = to;
    last_fail_ = reason;
    StateManager::instance().setConnectionState(to, reason);
}

void NetworkManager::connectionTaskEntry(void* arg)
{
    static_cast<NetworkManager*>(arg)->runConnectionStateMachine();
}

void NetworkManager::runConnectionStateMachine()
{
    ESP_LOGI(TAG, "Connection task started");

    while (started_) {
        switch (conn_state_) {
        case state::ConnectionState::OFFLINE:
            if (wifi_credentials_exist_) {
                transition(state::ConnectionState::WIFI_CONNECTING);
                wifi_->connectWithCredentials(cfg_.wifi_ssid.c_str(), cfg_.wifi_pass.c_str());
            } else {
                transition(state::ConnectionState::BLE_PROVISIONING);
            }
            break;

        case state::ConnectionState::WIFI_CONNECTING: {
            int timeout_ms = was_online_ ? 30000 : 10000;
            bool connected = false;
            for (int ms = 0; ms < timeout_ms && started_; ms += 200) {
                vTaskDelay(pdMS_TO_TICKS(200));
                if (wifi_->isConnected()) {
                    connected = true;
                    break;
                }
            }

            if (connected) {
                transition(state::ConnectionState::WIFI_CONNECTED);
            } else if (!was_online_) {
                transition(state::ConnectionState::BLE_PROVISIONING,
                           state::ConnectFailReason::WIFI_TIMEOUT);
            } else {
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::WIFI_TIMEOUT);
            }
            break;
        }

        case state::ConnectionState::WIFI_CONNECTED:
            transition(state::ConnectionState::WS_CONNECTING);
            break;

        case state::ConnectionState::WS_CONNECTING: {
            if (!ws_ || cfg_.ws_url.empty()) {
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::SERVER_UNREACHABLE);
                break;
            }

            esp_err_t err = ws_->connect(cfg_.ws_url.c_str());
            if (err != ESP_OK) {
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::SERVER_UNREACHABLE);
                break;
            }

            // Wait for auth to complete
            for (int ms = 0; ms < 15000 && started_; ms += 200) {
                vTaskDelay(pdMS_TO_TICKS(200));
                if (ws_->isAuthenticated()) break;
                if (!ws_->isConnected()) break;
            }

            if (ws_->isAuthenticated()) {
                reconnect_attempts_ = 0;
                was_online_ = true;
                transition(state::ConnectionState::ONLINE);
            } else {
                ws_->disconnect();
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::WS_HANDSHAKE_FAIL);
            }
            break;
        }

        case state::ConnectionState::WS_AUTHENTICATING:
            // Handled within WS_CONNECTING flow
            vTaskDelay(pdMS_TO_TICKS(100));
            break;

        case state::ConnectionState::ONLINE:
            // Wait for disconnect
            while (started_ && ws_ && ws_->isConnected() && ws_->isAuthenticated()) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }

            if (started_) {
                stopHeartbeat();
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::NONE);
            }
            break;

        case state::ConnectionState::RECONNECTING: {
            stopHeartbeat();

            auto policy = state::getRetryPolicy(last_fail_);

            if (was_online_ || reconnect_attempts_ < policy.max_retries) {
                uint32_t delay = state::calculateBackoff(policy,
                    was_online_ ? (reconnect_attempts_ % policy.max_retries)
                                : reconnect_attempts_);
                ESP_LOGW(TAG, "Reconnect attempt %d (delay %lums, reason=%d)",
                         reconnect_attempts_ + 1,
                         (unsigned long)delay, (int)last_fail_);

                vTaskDelay(pdMS_TO_TICKS(delay));
                reconnect_attempts_++;

                if (wifi_->isConnected()) {
                    transition(state::ConnectionState::WS_CONNECTING);
                } else {
                    transition(state::ConnectionState::WIFI_CONNECTING);
                    wifi_->connectWithCredentials(cfg_.wifi_ssid.c_str(), cfg_.wifi_pass.c_str());
                }
            } else if (policy.fallback_to_ble) {
                ESP_LOGW(TAG, "Max retries reached, falling back to BLE");
                reconnect_attempts_ = 0;
                transition(state::ConnectionState::BLE_PROVISIONING, last_fail_);
            } else {
                ESP_LOGW(TAG, "Max retries reached, going offline");
                reconnect_attempts_ = 0;
                transition(state::ConnectionState::OFFLINE);
                vTaskDelay(pdMS_TO_TICKS(30000));
            }
            break;
        }

        case state::ConnectionState::BLE_PROVISIONING:
            awaiting_provision_ = true;
            while (started_ && awaiting_provision_) {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            if (wifi_credentials_exist_) {
                transition(state::ConnectionState::WIFI_CONNECTING);
                wifi_->connectWithCredentials(cfg_.wifi_ssid.c_str(), cfg_.wifi_pass.c_str());
            }
            break;
        }
    }

    ESP_LOGW(TAG, "Connection task ended");
    vTaskDelete(nullptr);
}
