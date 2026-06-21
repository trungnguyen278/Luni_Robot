#include "PowerManager.hpp"
#include "Power.hpp"
#include "esp_log.h"

static const char* TAG = "PowerManager";

PowerManager::PowerManager(std::unique_ptr<Power> power, const Config& cfg)
    : power(std::move(power)), config(cfg) {}

PowerManager::~PowerManager() { stop(); }

bool PowerManager::init() {
    if (!power) {
        ESP_LOGE(TAG, "Power driver not provided!");
        return false;
    }
    return true;
}

void PowerManager::start() {
    if (started) return;
    started = true;
    running_ = true;
    // Own task (not the shared FreeRTOS timer service task): the report callback
    // does UART + WS/TLS sends, which need more stack than the ~2KB timer task.
    // Low priority (2) — battery is the least time-critical thing on the C5.
    xTaskCreatePinnedToCore(&PowerManager::taskEntry, "PowerMgr", 4096, this, 2, &task_, 0);
    ESP_LOGI(TAG, "PowerManager started");
}

void PowerManager::stop() {
    if (!started) return;
    started = false;
    running_ = false;
}

void PowerManager::taskEntry(void* arg) {
    static_cast<PowerManager*>(arg)->taskLoop();
}

void PowerManager::taskLoop() {
    // First sample shortly after boot, then on the configured interval.
    vTaskDelay(pdMS_TO_TICKS(1500));
    while (running_.load()) {
        evaluate();
        vTaskDelay(pdMS_TO_TICKS(config.evaluate_interval_ms));
    }
    task_ = nullptr;
    vTaskDelete(nullptr);
}

void PowerManager::sampleNow() { evaluate(); }

void PowerManager::evaluate() {
    if (!power) return;

    float   voltage  = power->getVoltage();
    uint8_t percent  = power->getBatteryPercent();
    uint8_t charging = power->isCharging();
    uint8_t full     = power->isFull();

    if (voltage < 0.0f || percent == BATTERY_INVALID) {
        // No battery / floating tap: report 255 so the S3 HIDES the icon — never
        // fake a value (R-DA-RBT-004). Safe even if the divider isn't wired yet.
        battery_present = false;
        reportIfChanged(BATTERY_INVALID, state::PowerState::NORMAL);
        return;
    }

    battery_present = true;
    last_voltage = voltage;

    if (config.enable_smoothing) {
        if (first_sample) { last_percent = percent; first_sample = false; }
        else {
            float smoothed = last_percent + config.smoothing_alpha * ((float)percent - last_percent);
            last_percent = static_cast<uint8_t>(smoothed);
        }
    } else {
        last_percent = percent;
    }

    ui_charging = (charging == 1);
    ui_full     = (full == 1);
    current_state = evaluateState(last_percent, charging, full);
    reportIfChanged(last_percent, current_state);
}

state::PowerState PowerManager::evaluateState(uint8_t percent, int chargingStatus, int fullStatus) {
    if (fullStatus == 1)     return state::PowerState::FULL;
    if (chargingStatus == 1) return state::PowerState::CHARGING;
    if (percent <= config.critical_percent) return state::PowerState::CRITICAL;
    return state::PowerState::NORMAL;
}

void PowerManager::reportIfChanged(uint8_t percent, state::PowerState st) {
    if (reported_once_ && percent == last_percent_sent && st == state_sent) return;
    reported_once_   = true;
    last_percent_sent = percent;
    state_sent        = st;
    ESP_LOGI(TAG, "Battery report: %u%% state=%d (%.2fV CHG:%d FULL:%d)",
             percent, static_cast<int>(st), last_voltage, ui_charging, ui_full);
    if (report_cb_) report_cb_(percent, st);
}
