#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <atomic>
#include "StateTypes.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class Power;

// PowerManager on the ESP32-C5 (battery ADC moved here from the S3, which had no
// free ADC pin). Samples battery voltage, smooths %, infers PowerState, and
// REPORTS via a callback — the C5 has no display, so DeviceProfile forwards the
// %+state to the S3 over UART (status-bar icon) and to the server. Reports 255
// (BATTERY_INVALID) when no battery is wired, so nothing is faked.
class PowerManager {
public:
    struct Config {
        uint32_t evaluate_interval_ms = 2000;
        float critical_percent = 8.0f;
        bool  enable_smoothing = true;
        float smoothing_alpha = 0.15f;
    };

    // percent: 0..100, or 255 (BATTERY_INVALID) = no battery present (hide on S3).
    using ReportCb = std::function<void(uint8_t percent, state::PowerState st)>;

    PowerManager(std::unique_ptr<Power> power, const Config& cfg);
    ~PowerManager();

    bool init();
    void start();
    void stop();

    void setReportCb(ReportCb cb) { report_cb_ = std::move(cb); }

    uint8_t getPercent() const { return last_percent; }
    state::PowerState getState() const { return current_state; }
    bool isBatteryPresent() const { return battery_present; }
    void sampleNow();

private:
    static void taskEntry(void* arg);
    void taskLoop();
    void evaluate();
    state::PowerState evaluateState(uint8_t percent, int chargingStatus, int fullStatus);
    void reportIfChanged(uint8_t percent, state::PowerState st);

    std::unique_ptr<Power> power;
    Config config;
    TaskHandle_t task_ = nullptr;
    std::atomic<bool> running_{false};
    bool started = false;

    float   last_voltage = 0.0f;
    uint8_t last_percent = 0;
    state::PowerState current_state = state::PowerState::NORMAL;

    // report dedup
    bool    reported_once_ = false;
    uint8_t last_percent_sent = 0;
    state::PowerState state_sent = state::PowerState::NORMAL;

    bool ui_charging = false;
    bool ui_full = false;
    bool battery_present = true;
    bool first_sample = true;

    ReportCb report_cb_;
};
