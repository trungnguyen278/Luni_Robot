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

// Voice-turn watchdog limits. The C5 owns the turn, so it is also the one
// that must guarantee the turn always terminates.
static constexpr uint32_t LISTEN_MAX_MS         = 30000;  // matches server's 30s buffer flush
static constexpr uint32_t PROCESSING_TIMEOUT_MS = 20000;  // STT+LLM+TTS should answer well within this
static constexpr uint32_t SPEAKING_STALL_MS     = 15000;  // no TTS data while SPEAKING → turn is dead
static constexpr uint32_t INTERACTION_WD_PERIOD_MS = 1000;

// Once a device has been online, `was_online_` keeps it retrying instead of
// falling back to BLE — good for blips, but a permanent change (WiFi password
// changed, router swapped, token revoked) would loop forever and lock the user
// out of re-provisioning. After this many failed reconnect attempts, release
// `was_online_` so the fallback_to_ble / OFFLINE branches become reachable.
static constexpr uint8_t ONLINE_RECONNECT_CYCLE_CAP = 6;

static inline uint32_t nowMs()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

NetworkManager::NetworkManager() = default;
NetworkManager::~NetworkManager()
{
    stop();
    if (inter_wd_sub_id_ != -1) {
        StateManager::instance().unsubscribeInteraction(inter_wd_sub_id_);
        inter_wd_sub_id_ = -1;
    }
}

bool NetworkManager::init(const Config& cfg)
{
    cfg_ = cfg;
    wifi_credentials_exist_ = !cfg_.wifi_ssid.empty();
    device_token_exists_ = !cfg_.device_token.empty();

    ESP_LOGI(TAG, "init: ws=%s mac=%s",
             cfg_.ws_url.c_str(), getDLuniceEfuseID());

    wifi_ = std::make_unique<WifiService>();
    ws_   = std::make_unique<WebSocketClient>();
    ota_  = std::make_unique<OtaManager>();
    sync_ = std::make_unique<DataSyncManager>();

    wifi_->init();
    ws_->init();
    ws_->setDeviceInfo(getDLuniceEfuseID(), cfg_.device_token.c_str(),
                       app_meta::APP_VERSION, app_meta::DLuniCE_MODEL);
    ESP_LOGI(TAG, "auth config: mac=%s token=%s",
             getDLuniceEfuseID(),
             cfg_.device_token.empty() ? "<empty>" : "<set>");
    ota_->init(StateManager::instance());
    sync_->init();

    setupWifi();
    setupWebSocket();

    // Track when the interaction state last changed, for the turn watchdog.
    inter_state_since_ms_ = nowMs();
    inter_wd_sub_id_ = StateManager::instance().subscribeInteraction(
        [this](state::InteractionState, state::InputSource) {
            inter_state_since_ms_ = nowMs();
        });

    return true;
}

void NetworkManager::start()
{
    if (started_) return;
    started_ = true;

    xTaskCreatePinnedToCore(&NetworkManager::connectionTaskEntry,
                            "net_conn", 4096, this, 5, &conn_task_, 0);

    if (!interaction_wd_timer_) {
        esp_timer_create_args_t args = {};
        args.callback = &NetworkManager::interactionWatchdogCb;
        args.arg = this;
        args.name = "inter_wd";
        esp_timer_create(&args, &interaction_wd_timer_);
        esp_timer_start_periodic(interaction_wd_timer_,
                                 (uint64_t)INTERACTION_WD_PERIOD_MS * 1000);
    }

    ESP_LOGI(TAG, "NetworkManager started");
}

void NetworkManager::stop()
{
    if (!started_) return;
    started_ = false;

    stopHeartbeat();

    if (interaction_wd_timer_) {
        esp_timer_stop(interaction_wd_timer_);
        esp_timer_delete(interaction_wd_timer_);
        interaction_wd_timer_ = nullptr;
    }

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
            ws_auth_failed_ = false;
            ESP_LOGI(TAG, "WS authenticated");

            // Reaching an authenticated cloud session is our health signal: a
            // freshly-OTA'd image that gets here works, so confirm it and stop
            // the bootloader from rolling it back on the next reset.
            if (ota_) ota_->confirmHealth();

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
        } else {
            ws_auth_failed_ = true;
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

    // Binary audio from server → relay to S3 via SPI.
    // One WS message = whole [2B len][opus] frames, but it arrives in
    // fragments. Dropping a single fragment mid-message would shift the
    // byte stream and corrupt every frame after it — so once any fragment
    // of a message is dropped, drop ALL its remaining fragments (sticky).
    // Whole-message drops are safe: alignment restarts at message boundary.
    ws_->onBinary([this](const uint8_t* data, size_t len, size_t offset, size_t total) {
        if (!data || len == 0 || !spi_bridge_) return;

        if (offset == 0) dl_msg_dropping_ = false;  // new message: clear sticky drop

        last_downlink_ms_ = nowMs();

        if (!speaking_session_active_) {
            auto s = StateManager::instance().getInteractionState();
            if (s == state::InteractionState::PROCESSING ||
                s == state::InteractionState::SPEAKING) {
                startSpeakingSession();
                StateManager::instance().setInteractionState(
                    state::InteractionState::SPEAKING, state::InputSource::SERVER_COMMAND);
            } else {
                dl_msg_dropping_ = true;
                return;
            }
        }

        if (dl_msg_dropping_) return;

        if (!spi_bridge_->sendAudioDownlink(data, len)) {
            dl_msg_dropping_ = true;
            ESP_LOGW(TAG, "DL fragment dropped (offset=%zu/%zu) — dropping rest of message to keep frame alignment",
                     offset, total);
        }
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

esp_err_t NetworkManager::sendBinaryWhole(const uint8_t* data, size_t len)
{
    if (ws_ && ws_connected_) return ws_->sendBinaryWhole(data, len);
    return ESP_FAIL;
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
    // EMOTION is emitted separately as a STRING (sendEmotionUpdate) — the numeric
    // enum here only covers 16/45 keys and the app casts `new` as a String, so a
    // number would be dropped (or throw). Skip it on the generic path.
    case state::StateEvent::Type::EMOTION:     return;
    default: return;
    }

    cJSON* msg = ws::createStateUpdate(category, event.old_value, event.new_value);
    if (msg && ws_) {
        ws_->sendMessage(msg);
    }
}

void NetworkManager::sendEmotionUpdate(const char* key)
{
    if (!key || !*key) return;
    cJSON* msg = ws::createStateUpdateStr("emotion", key);
    if (msg && ws_) {
        ws_->sendMessage(msg);
    }
}

void NetworkManager::sendOtaProgress(uint8_t percent, const char* phase)
{
    cJSON* msg = ws::createOtaProgress(percent, phase);
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

// === Interaction watchdog ===

void NetworkManager::interactionWatchdogCb(void* arg)
{
    static_cast<NetworkManager*>(arg)->checkInteraction();
}

void NetworkManager::checkInteraction()
{
    auto& sm = StateManager::instance();
    const auto s = sm.getInteractionState();
    if (s == state::InteractionState::IDLE) return;

    const uint32_t now = nowMs();
    const uint32_t in_state_ms = now - inter_state_since_ms_.load();

    switch (s) {
    case state::InteractionState::LISTENING:
        // Cap the turn (matters for wake-word turns, which have no release
        // event yet). Mirrors the server's own 30s buffer flush. No TX drain
        // here — this runs on the esp_timer task and must not block.
        if (in_state_ms > LISTEN_MAX_MS) {
            ESP_LOGW(TAG, "Watchdog: LISTENING %lums, forcing END",
                     (unsigned long)in_state_ms);
            sendText("END");
            sm.setInteractionState(state::InteractionState::PROCESSING,
                                   state::InputSource::SYSTEM);
        }
        break;

    case state::InteractionState::PROCESSING:
        // Server never answered (STT empty, pipeline error, packet lost).
        if (in_state_ms > PROCESSING_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Watchdog: PROCESSING %lums with no reply, going IDLE",
                     (unsigned long)in_state_ms);
            endSpeakingSession();
            sm.setInteractionState(state::InteractionState::IDLE,
                                   state::InputSource::SYSTEM);
        }
        break;

    case state::InteractionState::SPEAKING: {
        // Normal end-of-turn comes from S3's SPEAK_DONE; this only catches a
        // dead stream (S3 reboot, UART loss, server died mid-utterance).
        const uint32_t since_dl = now - last_downlink_ms_.load();
        if (in_state_ms > SPEAKING_STALL_MS && since_dl > SPEAKING_STALL_MS) {
            ESP_LOGW(TAG, "Watchdog: SPEAKING stalled (%lums no data), going IDLE",
                     (unsigned long)since_dl);
            endSpeakingSession();
            sm.setInteractionState(state::InteractionState::IDLE,
                                   state::InputSource::SYSTEM);
        }
        break;
    }

    default:
        break;
    }
}

// === Connection state machine ===

void NetworkManager::transition(state::ConnectionState to, state::ConnectFailReason reason)
{
    conn_state_ = to;
    last_fail_ = reason;
    if (!StateManager::instance().setConnectionState(to, reason)) {
        // The table rejected it: the UI (S3) and this state machine are now
        // out of sync. That is always a bug in the flow or the table — make
        // it loud instead of silently showing a stale screen for the rest of
        // the session.
        ESP_LOGE(TAG, "Connection transition rejected by StateManager: %d -> %d "
                      "(UI is now stale until the next accepted transition)",
                 (int)StateManager::instance().getConnectionState(), (int)to);
    }
}

state::ConnectFailReason NetworkManager::classifyWifiFailure() const
{
    const int reason = wifi_ ? wifi_->lastDisconnectReason() : 0;
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return state::ConnectFailReason::WIFI_NOT_FOUND;
    // Credential/4-way-handshake failures → almost always a wrong password.
    // Policy routes these straight to BLE re-provisioning (0 retries).
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
    case WIFI_REASON_ASSOC_FAIL:
        return state::ConnectFailReason::WIFI_AUTH_FAIL;
    default:
        // Associated at the link layer but no IP within the window → DHCP.
        if (wifi_ && wifi_->wasAssociated())
            return state::ConnectFailReason::DHCP_FAIL;
        return state::ConnectFailReason::WIFI_TIMEOUT;
    }
}

void NetworkManager::scanForProvisioning()
{
    if (!wifi_) return;
    wifi_->ensureStaStarted();
    auto nets = wifi_->scanNetworks();
    if (!nets.empty()) last_scan_ = std::move(nets);
    ESP_LOGI(TAG, "Provisioning scan: %u networks cached",
             (unsigned)last_scan_.size());
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
            // Provisioning is required when WiFi creds are missing OR the device
            // token is missing (e.g. paired to WiFi but never registered with the
            // server). Without the token, WS auth would fail, so go straight to
            // BLE instead of looping WiFi→auth-fail→retry.
            if (wifi_credentials_exist_ && device_token_exists_) {
                transition(state::ConnectionState::WIFI_CONNECTING);
                wifi_->connectWithCredentials(cfg_.wifi_ssid.c_str(), cfg_.wifi_pass.c_str());
            } else {
                scanForProvisioning();
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
                scanForProvisioning();
                transition(state::ConnectionState::BLE_PROVISIONING,
                           classifyWifiFailure());
            } else {
                // Classify so the retry policy can differ: wrong password
                // (WIFI_AUTH_FAIL) goes straight to BLE, AP-not-found / DHCP
                // get a few retries first.
                transition(state::ConnectionState::RECONNECTING,
                           classifyWifiFailure());
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

            ws_auth_failed_ = false;
            esp_err_t err = ws_->connect(cfg_.ws_url.c_str());
            if (err != ESP_OK) {
                transition(state::ConnectionState::RECONNECTING,
                           state::ConnectFailReason::SERVER_UNREACHABLE);
                break;
            }

            // Wait for auth to complete. Once the WS handshake completes (the
            // auth message is being sent), enter WS_AUTHENTICATING so the valid
            // transition path WS_CONNECTING(3) -> WS_AUTHENTICATING(4) -> ONLINE(5)
            // is followed. Skipping state 4 makes StateManager reject 3 -> 5 and
            // leaves the UI stuck on "server error" even after auth succeeds.
            bool saw_ws_connected = false;
            bool announced_authenticating = false;
            for (int ms = 0; ms < 15000 && started_; ms += 200) {
                vTaskDelay(pdMS_TO_TICKS(200));
                if (ws_->isAuthenticated()) break;
                if (ws_auth_failed_) break;
                if (ws_->isConnected()) {
                    saw_ws_connected = true;
                    if (!announced_authenticating) {
                        announced_authenticating = true;
                        transition(state::ConnectionState::WS_AUTHENTICATING);
                    }
                }
                if (saw_ws_connected && !ws_->isConnected()) break;
            }

            if (ws_->isAuthenticated()) {
                // Auth can complete inside the very first poll, before
                // isConnected() was ever observed — announce the intermediate
                // state anyway, or the WS_CONNECTING -> ONLINE jump gets
                // rejected by the transition table and the UI sticks on
                // "server error" for the whole online session.
                if (!announced_authenticating) {
                    transition(state::ConnectionState::WS_AUTHENTICATING);
                }
                reconnect_attempts_ = 0;
                was_online_ = true;
                transition(state::ConnectionState::ONLINE);
            } else {
                // A server auth_result with status != ok means the token is
                // bad/revoked — that will never succeed by retrying, so route it
                // to AUTH_REJECTED (→ BLE re-provisioning). Anything else (TCP
                // drop, handshake timeout) stays a retryable handshake failure.
                bool rejected = ws_->wasAuthRejected();
                ws_->disconnect();
                transition(state::ConnectionState::RECONNECTING,
                           rejected ? state::ConnectFailReason::AUTH_REJECTED
                                    : state::ConnectFailReason::WS_HANDSHAKE_FAIL);
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

            // A 0-retry policy (AUTH_REJECTED, WIFI_AUTH_FAIL) is a definitive
            // "this will never work" — don't retry at all (and never compute
            // `% max_retries`, which would divide by zero).
            bool give_up_now = (policy.max_retries == 0);

            // Break the was_online_ retry trap: after enough failed cycles, drop
            // was_online_ so a permanent change can fall back to BLE / OFFLINE.
            if (was_online_ && reconnect_attempts_ >= ONLINE_RECONNECT_CYCLE_CAP) {
                ESP_LOGW(TAG, "Reconnect exhausted (%u cycles) after being online "
                              "— releasing was_online_ to allow recovery",
                         (unsigned)reconnect_attempts_);
                was_online_ = false;
            }

            if (!give_up_now && (was_online_ || reconnect_attempts_ < policy.max_retries)) {
                uint8_t backoff_attempt = (policy.max_retries > 0)
                    ? (uint8_t)(reconnect_attempts_ % policy.max_retries)
                    : reconnect_attempts_;
                uint32_t delay = state::calculateBackoff(policy, backoff_attempt);
                // A clean drop from ONLINE (reason NONE) is usually a sub-second
                // network blip — retry almost immediately the first time instead
                // of sitting out the full backoff. Real outages fall through to
                // the normal backoff from attempt #2 on.
                if (was_online_ && reconnect_attempts_ == 0 &&
                    last_fail_ == state::ConnectFailReason::NONE) {
                    delay = 500;
                }
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
                ESP_LOGW(TAG, "%s, falling back to BLE provisioning",
                         give_up_now ? "Auth rejected" : "Max retries reached");
                reconnect_attempts_ = 0;
                was_online_ = false;  // fresh re-provision: treat as never-online
                scanForProvisioning();
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
