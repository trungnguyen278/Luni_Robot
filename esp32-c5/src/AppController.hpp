#pragma once
#include <memory>
#include <atomic>
#include <cstdint>
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace event {
    enum class AppEvent : uint8_t {
        USER_BUTTON,
        RELEASE_BUTTON,
        SLEEP_REQUEST,
        WAKE_REQUEST,
        REBOOT_REQUEST
    };
}

class NetworkManager;
class AudioManager;
class TouchInput;
class UartBridge;

// AppController for ESP32-C5: simplified orchestrator.
// No display, no BLE, no power manager, no OTA (those are on S3).
class AppController {
public:
    static AppController& instance();

    bool init();
    void start();
    void stop();
    void reboot();

    void postEvent(event::AppEvent evt);

    void attachModules(std::unique_ptr<AudioManager> audioIn,
                       std::unique_ptr<NetworkManager> networkIn,
                       std::unique_ptr<TouchInput> touchIn,
                       std::unique_ptr<UartBridge> uartIn);

    // Emotion parsing (delegated to NetworkManager)
    static state::EmotionState parseEmotionCode(const std::string& code);

private:
    AppController() = default;
    ~AppController();
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    static void controllerTask(void* param);
    void processQueue();

    void onInteractionStateChanged(state::InteractionState, state::InputSource);
    void onConnectivityStateChanged(state::ConnectivityState);
    void onSystemStateChanged(state::SystemState);

    int sub_inter_id = -1;
    int sub_conn_id  = -1;
    int sub_sys_id   = -1;

    std::unique_ptr<NetworkManager> network;
    std::unique_ptr<AudioManager>   audio;
    std::unique_ptr<TouchInput>     touch;
    std::unique_ptr<UartBridge>     uart;

    QueueHandle_t app_queue = nullptr;
    TaskHandle_t  app_task  = nullptr;
    std::atomic<bool> started{false};
};
