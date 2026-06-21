#pragma once
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"
#include "system/BluetoothService.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace event {
    enum class AppEvent : uint8_t {
        SLEEP_REQUEST,
        WAKE_REQUEST,
        REBOOT_REQUEST
    };
}

class NetworkManager;
class SpiBridge;
class UartBridge;

// AppController for ESP32-C5: network-only orchestrator.
// Audio, button, and display are now on S3. C5 handles WiFi/WS and
// relays data to/from S3 via SPI + UART.
class AppController {
public:
    static AppController& instance();

    bool init();
    void start();
    void stop();
    void reboot();

    void postEvent(event::AppEvent evt);
    void startNetwork();
    bool isNetworkStarted() const { return network_started_.load(); }

    void attachModules(std::unique_ptr<NetworkManager> networkIn,
                       std::unique_ptr<SpiBridge> spiIn,
                       std::unique_ptr<UartBridge> uartIn);

    // Context captured at boot (scanned WiFi list + current settings) used to
    // populate the BLE provisioning service when it later starts at runtime.
    void setProvisioningContext(std::vector<WifiInfo> networks,
                                BluetoothService::ConfigData cfg);

private:
    AppController() = default;
    ~AppController();
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    static void controllerTask(void* param);
    void processQueue();

    void onInteractionStateChanged(state::InteractionState, state::InputSource);
    void onConnectionStateChanged(state::ConnectionState, state::ConnectFailReason);
    void onSystemStateChanged(state::SystemState);
    void sendStatusHeartbeat();

    void startBleProvisioning();
    void stopBleProvisioning();

    int sub_inter_id = -1;
    int sub_conn_id  = -1;
    int sub_sys_id   = -1;

    std::unique_ptr<NetworkManager> network;
    std::unique_ptr<SpiBridge>      spi;
    std::unique_ptr<UartBridge>     uart;

    QueueHandle_t app_queue = nullptr;
    TaskHandle_t  app_task  = nullptr;
    std::atomic<bool> started{false};
    std::atomic<bool> network_started_{false};

    BluetoothService ble_;
    bool ble_active_ = false;
    std::vector<WifiInfo> prov_networks_;
    BluetoothService::ConfigData prov_cfg_;
};
