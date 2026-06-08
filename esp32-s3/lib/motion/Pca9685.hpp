#pragma once
#include <cstdint>
#include "driver/i2c_master.h"

// =============================================================================
// PCA9685 — 16-channel 12-bit PWM driver (I2C), used here to drive up to 8
// servos. Required on the ESP32-S3-CAM board because the camera consumes the
// low GPIOs and the S3 has only 8 LEDC channels (one already used by the LCD
// backlight) — not enough for 8 servos.
//
// Uses the modern ESP-IDF i2c_master bus API (IDF >= 5.2). The bus is created
// by the caller (MotionManager) and shared with the IMU.
// =============================================================================
class Pca9685
{
public:
    Pca9685() = default;
    ~Pca9685() = default;

    // Attach to an existing I2C master bus. addr = 7-bit (default 0x40).
    bool init(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz);

    // Set the PWM frequency for all channels (servos: 50 Hz).
    bool setPwmFreq(float hz);

    // Drive one channel with a pulse width in microseconds (analog servo).
    bool setChannelPulseUs(uint8_t channel, uint16_t pulse_us);

    // Fully turn a channel off (servo de-energised / no pulse).
    bool setChannelOff(uint8_t channel);

    // Turn every channel off.
    bool allOff();

    bool isReady() const { return dev_ != nullptr; }

private:
    bool writeReg(uint8_t reg, uint8_t val);
    bool readReg(uint8_t reg, uint8_t& val);
    // Raw 12-bit on/off counts for a channel.
    bool setChannelRaw(uint8_t channel, uint16_t on, uint16_t off);

    i2c_master_dev_handle_t dev_ = nullptr;
    float pwm_freq_hz_ = 50.0f;
};
