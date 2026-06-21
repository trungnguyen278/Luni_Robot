#pragma once

#include <cstdint>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "driver/gpio.h"

// =============================================================================
// Power — battery voltage sensing over ADC1 (+ optional TP4056 CHRG/FULL pins).
// MOVED from the ESP32-S3 to the C5: the S3-CAM board has no free ADC pin, so
// the battery divider is read on the C5 instead. Ported from the legacy
// `driver/adc.h` + `esp_adc_cal.h` API (deprecated, not reliable on the C5) to
// the modern `esp_adc/adc_oneshot` + `adc_cali` API.
// =============================================================================

#define BATTERY_INVALID 255
#define STATUS_UNKNOWN  255

class Power {
public:
    // adc_channel = ADC1 channel of the divider tap (C5: ADC_CHANNEL_0 = GPIO1).
    Power(adc_channel_t adc_channel,
          gpio_num_t pin_chg = GPIO_NUM_NC,
          gpio_num_t pin_full = GPIO_NUM_NC,
          float R1 = 10000,
          float R2 = 20000);

    Power(adc_channel_t adc_channel, float R1, float R2)
    : Power(adc_channel, GPIO_NUM_NC, GPIO_NUM_NC, R1, R2) {}

    ~Power();

    // Expose VBAT (volts) to PowerManager. -1.0 = floating/disconnected.
    float getVoltage() { return readVoltage(); }

    // 0–100% valid OR BATTERY_INVALID (255 = no battery / floating).
    uint8_t getBatteryPercent();

    // 1 = YES, 0 = NO, 255 = UNKNOWN (pin not wired).
    uint8_t isCharging();
    uint8_t isFull();

private:
    float readVoltage();
    uint8_t voltageToPercent(float v);

    adc_channel_t channel_;
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_cali_handle_t cali_handle_ = nullptr;
    bool cali_ok_ = false;

    gpio_num_t pin_chg_;
    gpio_num_t pin_full_;
    float R1_, R2_;
};
