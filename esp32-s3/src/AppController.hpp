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

class DisplayManager;
class PowerManager;
class AudioManager;
class SpiBridge;
class UartBridge;
class TouchInput;

// AppController for ESP32-S3: orchestrates Display, Power, Audio, SPI Bridge, Touch.
// S3 now owns audio + button. Interaction state is managed here and sent to C5 via SPI.
class AppController {
public:
    static AppController& instance();

    bool init();
    void start();
    void stop();
    void reboot();

    void postEvent(event::AppEvent evt);

    void attachModules(std::unique_ptr<DisplayManager> displayIn,
                       std::unique_ptr<PowerManager> powerIn,
                       std::unique_ptr<AudioManager> audioIn,
                       std::unique_ptr<SpiBridge> spiIn,
                       std::unique_ptr<UartBridge> uartIn,
                       std::unique_ptr<TouchInput> touchIn);

private:
    AppController() = default;
    ~AppController();
    AppController(const AppController&) = delete;
    AppController& operator=(const AppController&) = delete;

    static void controllerTask(void* param);
    void processQueue();

    void onInteractionStateChanged(state::InteractionState, state::InputSource);
    void onConnectionStateChanged(state::ConnectionState);
    void onSystemStateChanged(state::SystemState);
    void onPowerStateChanged(state::PowerState);
    void onEmotionStateChanged(state::EmotionState);

    // Independent S3 interaction watchdog. The turn contract gives the C5 the
    // primary watchdog (LISTENING 30s / PROCESSING 20s), but if the UART/C5 dies
    // WITHOUT pushing a state change, S3 would sit in LISTENING (mic open — a
    // privacy problem) or PROCESSING (stuck UI) forever. This is the backstop:
    // it runs on the AppCtrl task tick and forces IDLE if a state outlives a
    // generous cap. Thresholds are LONGER than the C5's so the C5 normally wins.
    void checkInteractionWatchdog();
    int64_t inter_since_us_ = 0;  // when the interaction state last changed
    static constexpr int64_t WD_LISTENING_MAX_MS  = 35000;  // > C5's 30s
    static constexpr int64_t WD_PROCESSING_MAX_MS = 25000;  // > C5's 20s
    static constexpr uint32_t WD_TICK_MS          = 1000;

    int sub_inter_id   = -1;
    int sub_conn_id    = -1;
    int sub_sys_id     = -1;
    int sub_power_id   = -1;
    int sub_emotion_id = -1;

    // Previous interaction state, to detect the SPEAKING -> IDLE edge that
    // means "TTS playback fully drained" (reported to C5 as SPEAK_DONE).
    state::InteractionState prev_inter_ = state::InteractionState::IDLE;

    std::unique_ptr<DisplayManager>   display;
    std::unique_ptr<PowerManager>     power;
    std::unique_ptr<AudioManager>     audio;
    std::unique_ptr<SpiBridge>        spi;
    std::unique_ptr<UartBridge>       uart;
    std::unique_ptr<TouchInput>       touch;

    QueueHandle_t app_queue = nullptr;
    TaskHandle_t  app_task  = nullptr;
    std::atomic<bool> started{false};
};
