#include "AppController.hpp"
#include "system/DisplayManager.hpp"
#include "system/PowerManager.hpp"
#include "system/AudioManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"
#include "TouchInput.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include <utility>

static const char* TAG = "AppController";

struct AppMessage {
    enum class Type : uint8_t {
        INTERACTION,
        CONNECTIVITY,
        SYSTEM,
        POWER,
        EMOTION,
        APP_EVENT
    } type;

    state::InteractionState  interaction_state;
    state::InputSource       interaction_source;
    state::ConnectionState   connection_state;
    state::SystemState       system_state;
    state::PowerState        power_state;
    state::EmotionState      emotion_state;
    event::AppEvent          app_event;
};

// === Singleton ===

AppController& AppController::instance()
{
    static AppController inst;
    return inst;
}

AppController::~AppController()
{
    stop();
    auto& sm = StateManager::instance();
    if (sub_inter_id != -1)   sm.unsubscribeInteraction(sub_inter_id);
    if (sub_conn_id != -1)    sm.unsubscribeConnectivity(sub_conn_id);
    if (sub_sys_id != -1)     sm.unsubscribeSystem(sub_sys_id);
    if (sub_power_id != -1)   sm.unsubscribePower(sub_power_id);
    if (sub_emotion_id != -1) sm.unsubscribeEmotion(sub_emotion_id);
    if (app_queue) { vQueueDelete(app_queue); app_queue = nullptr; }
}

// === Module attachment ===

void AppController::attachModules(std::unique_ptr<DisplayManager> displayIn,
                                   std::unique_ptr<PowerManager> powerIn,
                                   std::unique_ptr<AudioManager> audioIn,
                                   std::unique_ptr<SpiBridge> spiIn,
                                   std::unique_ptr<UartBridge> uartIn,
                                   std::unique_ptr<TouchInput> touchIn)
{
    if (started.load()) return;
    display = std::move(displayIn);
    power   = std::move(powerIn);
    audio   = std::move(audioIn);
    spi     = std::move(spiIn);
    uart    = std::move(uartIn);
    touch   = std::move(touchIn);
}

// === Lifecycle ===

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

    sub_conn_id = sm.subscribeConnectivity(
        [this](state::ConnectionState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::CONNECTIVITY;
            msg.connection_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_sys_id = sm.subscribeSystem(
        [this](state::SystemState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::SYSTEM;
            msg.system_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_power_id = sm.subscribePower(
        [this](state::PowerState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::POWER;
            msg.power_state = s;
            if (app_queue) xQueueSend(app_queue, &msg, 0);
        });

    sub_emotion_id = sm.subscribeEmotion(
        [this](state::EmotionState s) {
            AppMessage msg{}; msg.type = AppMessage::Type::EMOTION;
            msg.emotion_state = s;
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

    // Start display first and play full boot sequence before other modules
    if (display) {
        display->start();
        display->playBootSequence();
    }

    // Start remaining modules only after boot animation completes
    if (power) power->start();
    if (spi) spi->start();
    if (uart) {
        uart->start();
        uart->sendControlCmd(uart_proto::ControlCmd::BOOT_DONE);
        ESP_LOGI(TAG, "Sent BOOT_DONE to C5");
    }
    if (audio) audio->start();
    if (touch) touch->start();

    ESP_LOGI(TAG, "AppController started");
}

void AppController::stop()
{
    if (!started.load()) return;
    started.store(false);

    if (audio) audio->stop();
    if (touch) touch->stop();
    if (uart) uart->stop();
    if (spi) spi->stop();
    if (display) display->stop();
    if (power) power->stop();

    vTaskDelay(pdMS_TO_TICKS(100));
    app_task = nullptr;
    ESP_LOGI(TAG, "AppController stopped");
}

void AppController::reboot()
{
    ESP_LOGW(TAG, "Reboot requested");
    if (uart) uart->sendControlCmd(uart_proto::ControlCmd::REBOOT);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

void AppController::postEvent(event::AppEvent evt)
{
    if (!app_queue) return;
    AppMessage msg{}; msg.type = AppMessage::Type::APP_EVENT; msg.app_event = evt;
    xQueueSend(app_queue, &msg, 0);
}

// === Task & Queue ===

void AppController::controllerTask(void* param)
{
    static_cast<AppController*>(param)->processQueue();
}

void AppController::processQueue()
{
    ESP_LOGI(TAG, "Controller task started");
    AppMessage msg{};
    inter_since_us_ = esp_timer_get_time();

    while (started.load()) {
        // Wake at least every WD_TICK_MS even with no messages so the interaction
        // watchdog can fire when the C5/UART goes silent without a state change.
        if (xQueueReceive(app_queue, &msg, pdMS_TO_TICKS(WD_TICK_MS)) != pdTRUE) {
            checkInteractionWatchdog();
            continue;
        }
        checkInteractionWatchdog();
        {
            switch (msg.type) {
            case AppMessage::Type::INTERACTION:
                inter_since_us_ = esp_timer_get_time();
                onInteractionStateChanged(msg.interaction_state, msg.interaction_source);
                break;
            case AppMessage::Type::CONNECTIVITY:
                onConnectionStateChanged(msg.connection_state);
                break;
            case AppMessage::Type::SYSTEM:
                onSystemStateChanged(msg.system_state);
                break;
            case AppMessage::Type::POWER:
                onPowerStateChanged(msg.power_state);
                break;
            case AppMessage::Type::EMOTION:
                onEmotionStateChanged(msg.emotion_state);
                break;
            case AppMessage::Type::APP_EVENT:
                switch (msg.app_event) {
                case event::AppEvent::USER_BUTTON:
                    // Button press: check connectivity (received from C5 via SPI)
                    if (StateManager::instance().getConnectionState() != state::ConnectionState::ONLINE) {
                        ESP_LOGW(TAG, "Button ignored - not online");
                        break;
                    }
                    if (audio && StateManager::instance().getInteractionState() == state::InteractionState::SPEAKING) {
                        audio->stopSpeaking(true);
                    }
                    StateManager::instance().setInteractionState(
                        state::InteractionState::LISTENING, state::InputSource::BUTTON);
                    break;
                case event::AppEvent::RELEASE_BUTTON:
                    if (StateManager::instance().getInteractionState() == state::InteractionState::LISTENING) {
                        StateManager::instance().setInteractionState(
                            state::InteractionState::IDLE, state::InputSource::BUTTON);
                    }
                    break;
                case event::AppEvent::REBOOT_REQUEST:
                    reboot();
                    break;
                case event::AppEvent::SLEEP_REQUEST:
                case event::AppEvent::WAKE_REQUEST:
                    break;
                }
                break;
            }
        }
    }

    vTaskDelete(nullptr);
}

// === State callbacks ===

void AppController::onInteractionStateChanged(state::InteractionState s, state::InputSource src)
{
    ESP_LOGI(TAG, "Interaction: %d (src=%d)", (int)s, (int)src);

    const auto prev = prev_inter_;
    prev_inter_ = s;

    // Forward interaction state to C5 via UART.
    // Trigger sources that own a listening turn: BUTTON (legacy) or WAKEWORD (now).
    if (uart) {
        bool trig = (src == state::InputSource::BUTTON ||
                     src == state::InputSource::WAKEWORD);
        if (s == state::InteractionState::LISTENING && trig) {
            // Carry the trigger source so C5 records WAKEWORD turns as such.
            uint8_t payload[1] = { (uint8_t)src };
            uart->sendControlCmd(uart_proto::ControlCmd::START, payload, 1);
        } else if (s == state::InteractionState::IDLE && trig) {
            uart->sendControlCmd(uart_proto::ControlCmd::END);
        } else if (s == state::InteractionState::IDLE && !trig &&
                   prev == state::InteractionState::SPEAKING) {
            // Playback drained locally (AudioManager) → close the turn on C5.
            // Echo-safe: if C5 is already IDLE this is a no-op there.
            uart->sendControlCmd(uart_proto::ControlCmd::SPEAK_DONE);
        }
    }

    switch (s) {
    case state::InteractionState::TRIGGERED:
        StateManager::instance().setInteractionState(state::InteractionState::LISTENING, src);
        break;
    case state::InteractionState::CANCELLING:
        StateManager::instance().setInteractionState(state::InteractionState::IDLE, state::InputSource::UNKNOWN);
        break;
    default:
        break;
    }
}

void AppController::checkInteractionWatchdog()
{
    auto& sm = StateManager::instance();
    const auto s = sm.getInteractionState();
    // Only LISTENING (mic open = privacy) and PROCESSING (stuck UI) need the
    // backstop. SPEAKING is already covered by AudioManager's stale-audio timeout.
    if (s != state::InteractionState::LISTENING &&
        s != state::InteractionState::PROCESSING) return;

    const int64_t age_ms = (esp_timer_get_time() - inter_since_us_) / 1000;
    const int64_t cap = (s == state::InteractionState::LISTENING)
        ? WD_LISTENING_MAX_MS : WD_PROCESSING_MAX_MS;
    if (age_ms < cap) return;

    ESP_LOGW(TAG, "S3 watchdog: %s stuck %lldms (C5 silent) — forcing IDLE",
             s == state::InteractionState::LISTENING ? "LISTENING" : "PROCESSING",
             (long long)age_ms);

    // Recover locally: cut the mic / drain audio and drop to IDLE. src=SYSTEM so
    // this isn't read as a user/wakeword release. If the C5 is alive it will
    // re-sync S3 on its next state push (S3 mirrors the C5); if the C5 is the
    // thing that died, S3 has at least closed the open mic. No UART nudge — the
    // C5 has its own turn watchdog and forcing its state from here risks desync.
    if (audio) audio->stopAll();
    sm.setInteractionState(state::InteractionState::IDLE, state::InputSource::SYSTEM);
    inter_since_us_ = esp_timer_get_time();
}

void AppController::onConnectionStateChanged(state::ConnectionState s)
{
    ESP_LOGI(TAG, "Connection: %d", (int)s);

    // Stop audio if not ONLINE — no point streaming mic when C5 can't relay to server
    if (s != state::ConnectionState::ONLINE && audio) {
        auto interaction = StateManager::instance().getInteractionState();
        if (interaction == state::InteractionState::LISTENING ||
            interaction == state::InteractionState::PROCESSING) {
            audio->stopAll();
            StateManager::instance().setInteractionState(
                state::InteractionState::IDLE, state::InputSource::UNKNOWN);
        }
    }
}

void AppController::onSystemStateChanged(state::SystemState s)
{
    ESP_LOGI(TAG, "System: %d", (int)s);

    if (s == state::SystemState::ERROR && audio) {
        audio->stopAll();
        StateManager::instance().setInteractionState(
            state::InteractionState::IDLE, state::InputSource::UNKNOWN);
    }
}

void AppController::onPowerStateChanged(state::PowerState s)
{
    ESP_LOGI(TAG, "Power: %d", (int)s);
}

void AppController::onEmotionStateChanged(state::EmotionState s)
{
    ESP_LOGI(TAG, "Emotion: %d", (int)s);
}
