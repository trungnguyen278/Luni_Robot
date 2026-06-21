#pragma once
#include <cstdint>
#include "driver/i2c_master.h"

// =============================================================================
// MPU6050 — 6-axis IMU (accel + gyro) over I2C. Shares the same bus as the
// PCA9685 (different address, default 0x68). Used for balance / fall / tap
// detection on the Otto-style robot.
// =============================================================================
class Mpu6050
{
public:
    struct Sample {
        float ax, ay, az;   // g
        float gx, gy, gz;   // deg/s
        float temp_c;       // °C
    };

    Mpu6050() = default;
    ~Mpu6050() = default;

    bool init(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz);

    // Verify WHO_AM_I (== 0x68).
    bool probe();

    // Read all 14 sensor bytes in one burst and convert to physical units.
    bool read(Sample& out);

    bool isReady() const { return dev_ != nullptr; }

private:
    bool writeReg(uint8_t reg, uint8_t val);
    bool readRegs(uint8_t reg, uint8_t* buf, size_t len);

    i2c_master_dev_handle_t dev_ = nullptr;
    float accel_scale_ = 16384.0f;  // LSB/g  for ±2g
    float gyro_scale_  = 131.0f;    // LSB/(deg/s) for ±250 dps
};
