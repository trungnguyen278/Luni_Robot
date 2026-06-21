#include "Pca9685.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "Pca9685";

// Register map
static constexpr uint8_t REG_MODE1     = 0x00;
static constexpr uint8_t REG_MODE2     = 0x01;
static constexpr uint8_t REG_LED0_ON_L = 0x06;  // base; each channel = +4*ch
static constexpr uint8_t REG_PRESCALE  = 0xFE;

// MODE1 bits
static constexpr uint8_t MODE1_RESTART = 0x80;
static constexpr uint8_t MODE1_AI      = 0x20;  // auto-increment
static constexpr uint8_t MODE1_SLEEP   = 0x10;
static constexpr uint8_t MODE1_ALLCALL = 0x01;
// MODE2 bits
static constexpr uint8_t MODE2_OUTDRV  = 0x04;  // totem-pole output

static constexpr int I2C_TIMEOUT_MS = 50;
static constexpr float OSC_CLOCK_HZ = 25000000.0f;  // internal 25 MHz oscillator

bool Pca9685::init(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz)
{
    if (!bus) return false;

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = addr;
    dev_cfg.scl_speed_hz    = scl_hz;

    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &dev_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add_device(0x%02X) failed: %s", addr, esp_err_to_name(err));
        dev_ = nullptr;
        return false;
    }

    // Reset to a known state: auto-increment + all-call, totem-pole outputs.
    if (!writeReg(REG_MODE1, MODE1_AI | MODE1_ALLCALL)) return false;
    if (!writeReg(REG_MODE2, MODE2_OUTDRV)) return false;
    vTaskDelay(pdMS_TO_TICKS(5));

    if (!setPwmFreq(pwm_freq_hz_)) return false;
    allOff();

    ESP_LOGI(TAG, "PCA9685 ready at 0x%02X, %.0f Hz", addr, pwm_freq_hz_);
    return true;
}

bool Pca9685::setPwmFreq(float hz)
{
    if (!dev_ || hz < 24.0f || hz > 1526.0f) return false;
    pwm_freq_hz_ = hz;

    // prescale = round(osc / (4096 * freq)) - 1
    uint8_t prescale = (uint8_t)(std::lround(OSC_CLOCK_HZ / (4096.0f * hz)) - 1);

    uint8_t old_mode = 0;
    if (!readReg(REG_MODE1, old_mode)) return false;

    uint8_t sleep_mode = (old_mode & ~MODE1_RESTART) | MODE1_SLEEP;
    if (!writeReg(REG_MODE1, sleep_mode)) return false;      // sleep to set prescale
    if (!writeReg(REG_PRESCALE, prescale)) return false;
    if (!writeReg(REG_MODE1, old_mode)) return false;        // wake
    vTaskDelay(pdMS_TO_TICKS(1));
    if (!writeReg(REG_MODE1, old_mode | MODE1_RESTART | MODE1_AI)) return false;

    return true;
}

bool Pca9685::setChannelRaw(uint8_t channel, uint16_t on, uint16_t off)
{
    if (!dev_ || channel > 15) return false;
    uint8_t reg = REG_LED0_ON_L + 4 * channel;
    uint8_t buf[5] = {
        reg,
        (uint8_t)(on & 0xFF), (uint8_t)(on >> 8),
        (uint8_t)(off & 0xFF), (uint8_t)(off >> 8)
    };
    return i2c_master_transmit(dev_, buf, sizeof(buf), I2C_TIMEOUT_MS) == ESP_OK;
}

bool Pca9685::setChannelPulseUs(uint8_t channel, uint16_t pulse_us)
{
    if (!dev_) return false;
    // counts = pulse_us * 4096 * freq / 1e6
    float counts = (float)pulse_us * 4096.0f * pwm_freq_hz_ / 1000000.0f;
    uint16_t off = (uint16_t)(counts + 0.5f);
    if (off > 4095) off = 4095;
    return setChannelRaw(channel, 0, off);
}

bool Pca9685::setChannelOff(uint8_t channel)
{
    // Bit 12 (0x1000) in OFF = full-off.
    return setChannelRaw(channel, 0, 0x1000);
}

bool Pca9685::allOff()
{
    if (!dev_) return false;
    bool ok = true;
    for (uint8_t ch = 0; ch < 16; ++ch) ok &= setChannelOff(ch);
    return ok;
}

bool Pca9685::writeReg(uint8_t reg, uint8_t val)
{
    if (!dev_) return false;
    uint8_t buf[2] = { reg, val };
    esp_err_t err = i2c_master_transmit(dev_, buf, sizeof(buf), I2C_TIMEOUT_MS);
    if (err != ESP_OK) ESP_LOGW(TAG, "writeReg 0x%02X failed: %s", reg, esp_err_to_name(err));
    return err == ESP_OK;
}

bool Pca9685::readReg(uint8_t reg, uint8_t& val)
{
    if (!dev_) return false;
    esp_err_t err = i2c_master_transmit_receive(dev_, &reg, 1, &val, 1, I2C_TIMEOUT_MS);
    if (err != ESP_OK) ESP_LOGW(TAG, "readReg 0x%02X failed: %s", reg, esp_err_to_name(err));
    return err == ESP_OK;
}
