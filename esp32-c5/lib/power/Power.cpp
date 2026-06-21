#include "Power.hpp"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "Power";

Power::Power(adc_channel_t adc_channel, gpio_num_t pin_chg, gpio_num_t pin_full, float R1, float R2)
    : channel_(adc_channel), pin_chg_(pin_chg), pin_full_(pin_full), R1_(R1), R2_(R2)
{
    adc_oneshot_unit_init_cfg_t init_cfg = {};
    init_cfg.unit_id = ADC_UNIT_1;
    if (adc_oneshot_new_unit(&init_cfg, &adc_handle_) != ESP_OK) {
        ESP_LOGE(TAG, "adc_oneshot_new_unit failed");
        adc_handle_ = nullptr;
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.atten    = ADC_ATTEN_DB_12;          // ~0..3.1V at the pin
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    adc_oneshot_config_channel(adc_handle_, channel_, &chan_cfg);

    // Curve-fitting calibration (supported on the C5). Falls back to a linear
    // approximation if the eFuse calibration bits are missing.
    adc_cali_curve_fitting_config_t cali_cfg = {};
    cali_cfg.unit_id  = ADC_UNIT_1;
    cali_cfg.chan     = channel_;
    cali_cfg.atten    = ADC_ATTEN_DB_12;
    cali_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    cali_ok_ = (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle_) == ESP_OK);
    if (!cali_ok_) {
        ESP_LOGW(TAG, "ADC calibration unavailable; using raw scaling");
    }

    if (pin_chg_ != GPIO_NUM_NC) {
        gpio_config_t io = {};
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.pin_bit_mask = (1ULL << pin_chg_);
        gpio_config(&io);
    }
    if (pin_full_ != GPIO_NUM_NC) {
        gpio_config_t io = {};
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.pin_bit_mask = (1ULL << pin_full_);
        gpio_config(&io);
    }
}

Power::~Power()
{
    if (cali_handle_) adc_cali_delete_scheme_curve_fitting(cali_handle_);
    if (adc_handle_)  adc_oneshot_del_unit(adc_handle_);
}

// Read VBAT real voltage, return -1 if disconnected/floating.
float Power::readVoltage()
{
    if (!adc_handle_) return -1.0f;

    int raw = 0;
    if (adc_oneshot_read(adc_handle_, channel_, &raw) != ESP_OK) return -1.0f;

    int mv = 0;
    if (cali_ok_) {
        if (adc_cali_raw_to_voltage(cali_handle_, raw, &mv) != ESP_OK) return -1.0f;
    } else {
        // Uncalibrated fallback: 12-bit raw over ~3.3V full-scale at 12dB atten.
        mv = (int)((float)raw * 3300.0f / 4095.0f);
    }

    // < 40mV at the tap -> impossible for a real divided battery => floating.
    if (mv < 40) return -1.0f;

    float vadc = mv / 1000.0f;
    float vbat = vadc * ((R1_ + R2_) / R2_);
    return vbat;
}

uint8_t Power::voltageToPercent(float v)
{
    struct { float v; uint8_t p; } tbl[] = {
        {3.00, 0}, {3.30, 10}, {3.50, 25}, {3.70, 50},
        {3.90, 75}, {4.10, 90}, {4.20, 100}
    };

    uint8_t raw_percent = 0;
    if (v <= tbl[0].v) raw_percent = 0;
    else if (v >= tbl[6].v) raw_percent = 100;
    else {
        for (int i = 0; i < 6; i++) {
            if (v >= tbl[i].v && v < tbl[i + 1].v) {
                float r = (v - tbl[i].v) / (tbl[i + 1].v - tbl[i].v);
                raw_percent = tbl[i].p + r * (tbl[i + 1].p - tbl[i].p);
                break;
            }
        }
    }

    // Hysteresis: only update when difference >= 5%.
    static int last_percent = -1;
    if (last_percent < 0) {
        last_percent = raw_percent;
    } else if (std::abs((int)raw_percent - last_percent) >= 5) {
        last_percent = raw_percent;
    }

    int rounded = (last_percent / 5) * 5;   // round DOWN to nearest 5%
    if (rounded > 100) rounded = 100;
    return static_cast<uint8_t>(rounded);
}

uint8_t Power::getBatteryPercent()
{
    float v = readVoltage();
    if (v < 0.0f) return BATTERY_INVALID;
    return voltageToPercent(v);
}

// TP4056: CHRG LOW = charging.
uint8_t Power::isCharging()
{
    if (pin_chg_ == GPIO_NUM_NC) return STATUS_UNKNOWN;
    return gpio_get_level(pin_chg_) == 0 ? 1 : STATUS_UNKNOWN;
}

// TP4056: FULL LOW = battery full.
uint8_t Power::isFull()
{
    if (pin_full_ == GPIO_NUM_NC) return STATUS_UNKNOWN;
    return gpio_get_level(pin_full_) == 0 ? 1 : STATUS_UNKNOWN;
}
