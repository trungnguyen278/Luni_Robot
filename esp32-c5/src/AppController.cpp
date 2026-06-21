#include "AppController.hpp"
#include "system/NetworkManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <utility>

static const char* TAG = "AppController";

struct AppMessage {
    enum class Type : uint8_t {
        INTERACTION,
        CONNECTION,
        SYSTEM,
        APP_EVENT
    } type;

    state::InteractionState  interaction_state;
    state::InputSource       interaction_source;
    state::ConnectionState   connection_state;
    state::ConnectFailReason fail_reason;
    state::SystemState       system_state;
    event::AppEvent          app_event;
};

AppController& AppController::instance()
{
    static AppController inst;
    return inst;
}

AppController::~AppController()
{
    stop();
    auto& sm = StateManager::instance();
    if (sub_inter_id != -1) sm.unsubscribeInteraction(sub_inter_id);
    if (sub_conn_id != -1)  sm.unsubscribeConnection(sub_conn_id);
    if (sub_sys_id != -1)   sm.unsubscribeSystem(sub_sys_id);
    if (app_queue) { vQueueDelete(app_queue); app_queue = nullptr; }
}

void AppController::attachModules(std::unique_ptr<NetworkManager> networkIn,
                                   std::unique_ptr<SpiBridge> spiIn,
                                   std::unique_ptr<UartBridge> uartIn)
{
    if (started.load()) return;
    network = std::move(networkIn);
    spi     = std::move(spiIn);
    uart    = std::move(uartIn);
}

void AppController::setProvisioningContext(std::vector<WifiInfo> networks,
                                           BluetoothService::ConfigData cfg)
{
    prov_networks_ = std::move(networks);
    prov_cfg_ = std::move(cfg);
}

bool AppController::init()
{
    ESP_LOGI(TAG, "init()");

    app_queue = xQueueCreate(16, sizeof(AppMessage));
    if (!app_queue) {
        ESP_LOGE(TAG, "Failed to create queue");
        return false;
    }

    auto& sm = StateManager::instance();

    sub_inter_id = sm.subscribeInteraction(
        [this](state::InteractionState s, state::InputSource src) {
            AppMessage msg{}; msg.type = AppMessage::Type::INTERACTION;
            msg.interaction_state = s; msg.interaction_source = src;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_conn_id = sm.subscribeConnection(
        [this](state::ConnectionState s, state::ConnectFailReason reason) {
            AppMessage msg{}; msg.type = AppMessage::Type::CONNECTION;
            msg.connection_state = s;
            msg.fail_reason = reason;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_sys_id = sm.subscribeSystem(
        [this](state::SystemState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::SYSTEM;
            msg.system_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    return true;
}

void AppController::start()
{
    if (started.load()) return;
    started.store(true);

    xTaskCreatePinnedToCore(&AppController::controllerTask, "AppCtrl", 4096, this, 4, &app_task, 0);
    vTaskDelay(pdMS_TO_TICKS(10));

    if (uart) uart->start();
    if (spi) spi->start();

    xTaskCreate([](void* arg) {
        vTaskDelay(pdMS_TO_TICKS(20000));
        auto* self = static_cast<AppController*>(arg);
        if (!self->isNetworkStarted()) {
            ESP_LOGW(TAG, "S3 BOOT_DONE timeout, starting network anyway");
            self->startNetwork();
        }
        vTaskDelete(nullptr);
    }, "BootWait", 2048, this, 2, nullptr);

    ESP_LOGI(TAG, "AppController started (network deferred until S3 BOOT_DONE)");
}

void AppController::startNetwork()
{
    if (network_started_.exchange(true)) return;
    if (network) {
        network->start();
        ESP_LOGI(TAG, "Network started");
    }
}

void AppController::stop()
{
    if (!started.load()) return;
    started.store(false);

    if (uart) uart->stop();
    if (network) network->stop();
    if (spi) spi->stop();

    vTaskDelay(pdMS_TO_TICKS(100));
    app_task = nullptr;
    ESP_LOGI(TAG, "AppController stopped");
}

void AppController::reboot()
{
    ESP_LOGW(TAG, "Reboot requested");
    esp_restart();
}

void AppController::postEvent(event::AppEvent evt)
{
    if (!app_queue) return;
    AppMessage msg{}; msg.type = AppMessage::Type::APP_EVENT; msg.app_event = evt;
    xQueueSend(app_queue, &msg, 0);
}

void AppController::controllerTask(void* param)
{
    static_cast<AppController*>(param)->processQueue();
}

void AppController::sendStatusHeartbeat()
{
    if (!uart) return;
    auto& sm = StateManager::instance();
    uart->sendStatusUpdate(
        (uint8_t)sm.getInteractionState(),
        (uint8_t)sm.getConnectionState(),
        (uint8_t)sm.getSystemState(),
        (uint8_t)sm.getEmotionState(),
        (uint8_t)sm.getLastFailReason());
}

void AppController::processQueue()
{
    ESP_LOGI(TAG, "Controller task started");
    AppMessage msg{};

    while (started.load()) {
        if (xQueueReceive(app_queue, &msg, pdMS_TO_TICKS(3000)) == pdTRUE) {
            switch (msg.type) {
            case AppMessage::Type::INTERACTION:
                onInteractionStateChanged(msg.interaction_state, msg.interaction_source);
                break;
            case AppMessage::Type::CONNECTION:
                onConnectionStateChanged(msg.connection_state, msg.fail_reason);
                break;
            case AppMessage::Type::SYSTEM:
                onSystemStateChanged(msg.system_state);
                break;
            case AppMessage::Type::APP_EVENT:
                switch (msg.app_event) {
                case event::AppEvent::REBOOT_REQUEST:
                    reboot();
                    break;
                case event::AppEvent::SLEEP_REQUEST:
                case event::AppEvent::WAKE_REQUEST:
                    break;
                }
                break;
            }
        } else {
            sendStatusHeartbeat();
        }
    }

    vTaskDelete(nullptr);
}

void AppController::onInteractionStateChanged(state::InteractionState s, state::InputSource src)
{
    ESP_LOGI(TAG, "Interaction: %d (src=%d)", (int)s, (int)src);

    if (uart) {
        uart->sendStatusUpdate(
            (uint8_t)s,
            (uint8_t)StateManager::instance().getConnectionState(),
            (uint8_t)StateManager::instance().getSystemState(),
            (uint8_t)StateManager::instance().getEmotionState(),
            (uint8_t)StateManager::instance().getLastFailReason());
    }
}

void AppController::onConnectionStateChanged(state::ConnectionState s,
                                             state::ConnectFailReason reason)
{
    ESP_LOGI(TAG, "Connection: %d (reason=%d)", (int)s, (int)reason);

    if (s == state::ConnectionState::BLE_PROVISIONING) {
        startBleProvisioning();
    } else if (ble_active_ && s != state::ConnectionState::BLE_CONNECTED) {
        stopBleProvisioning();
    }

    if (uart) {
        uart->sendStatusUpdate(
            (uint8_t)StateManager::instance().getInteractionState(),
            (uint8_t)s,
            (uint8_t)StateManager::instance().getSystemState(),
            (uint8_t)StateManager::instance().getEmotionState(),
            (uint8_t)reason);
    }
}

void AppController::startBleProvisioning()
{
    if (ble_active_) return;

    // Prefer the freshest scan: NetworkManager captures a live scan right before
    // entering BLE provisioning at runtime; fall back to the boot scan otherwise.
    const std::vector<WifiInfo>* networks = &prov_networks_;
    if (network && !network->scannedNetworks().empty()) {
        networks = &network->scannedNetworks();
    }

    if (!ble_.init("Luni", *networks, &prov_cfg_)) {
        ESP_LOGE(TAG, "BLE init failed");
        return;
    }

    ble_.onConnectionChange([this](bool connected) {
        if (connected) {
            StateManager::instance().setConnectionState(state::ConnectionState::BLE_CONNECTED);
        } else if (ble_active_) {
            StateManager::instance().setConnectionState(state::ConnectionState::BLE_PROVISIONING);
        }
    });

    ble_.onConfigComplete([this](const BluetoothService::ConfigData& cfg) {
        ESP_LOGI(TAG, "BLE config received: ssid='%s'", cfg.ssid.c_str());

        nvs_handle_t h;
        if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            if (!cfg.ssid.empty())
                nvs_set_str(h, "ssid", cfg.ssid.c_str());
            if (!cfg.pass.empty())
                nvs_set_str(h, "pass", cfg.pass.c_str());
            if (!cfg.ws_url.empty())
                nvs_set_str(h, "ws_url", cfg.ws_url.c_str());
            if (!cfg.user_id.empty())
                nvs_set_str(h, "user_id", cfg.user_id.c_str());
            if (!cfg.device_token.empty())
                nvs_set_str(h, "device_token", cfg.device_token.c_str());
            if (!cfg.admin_secret.empty())
                nvs_set_str(h, "admin_secret", cfg.admin_secret.c_str());
            nvs_commit(h);
            nvs_close(h);
        }

        // Persist only — do NOT tear down BLE or connect live here. The app's
        // pairing flow expects to keep the BLE link after COMMIT to send the
        // RESTART command; on reboot the device reads the new creds + token from
        // NVS and connects normally (device_token now present → no auth-fail loop).
        // Releasing provisioning here would drop the app's connection mid-flow.
        ESP_LOGI(TAG, "Provisioning saved to NVS; awaiting app RESTART");
    });

    ble_.start();
    ble_active_ = true;

    if (uart) {
        const char* pin = ble_.getPairingPin();
        uart->sendDeviceCmd(uart_proto::ControlCmd::BLE_PIN,
                            (const uint8_t*)pin, strlen(pin));
    }

    ESP_LOGI(TAG, "BLE provisioning started");
}

void AppController::stopBleProvisioning()
{
    if (!ble_active_) return;
    ble_.deinit();
    ble_active_ = false;
    ESP_LOGI(TAG, "BLE provisioning stopped");
}

void AppController::onSystemStateChanged(state::SystemState s)
{
    ESP_LOGI(TAG, "System: %d", (int)s);

    if (uart) {
        uart->sendStatusUpdate(
            (uint8_t)StateManager::instance().getInteractionState(),
            (uint8_t)StateManager::instance().getConnectionState(),
            (uint8_t)s,
            (uint8_t)StateManager::instance().getEmotionState(),
            (uint8_t)StateManager::instance().getLastFailReason());
    }
}
