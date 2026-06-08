#include "Mpu6050.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char* TAG = "Mpu6050";

// Register map
static constexpr uint8_t REG_SMPLRT_DIV  = 0x19;
static constexpr uint8_t REG_CONFIG      = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;  // accel(6) temp(2) gyro(6)
static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
static constexpr uint8_t REG_WHO_AM_I     = 0x75;

static constexpr int I2C_TIMEOUT_MS = 50;

bool Mpu6050::init(i2c_master_bus_handle_t bus, uint8_t addr, uint32_t scl_hz)
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

    if (!probe()) {
        ESP_LOGW(TAG, "WHO_AM_I mismatch (IMU not present?)");
        // keep going; caller decides whether this is fatal
    }

    // Wake up (clear SLEEP), select gyro PLL clock.
    if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
    writeReg(REG_SMPLRT_DIV, 0x07);    // 1kHz / (1+7) = 125 Hz
    writeReg(REG_CONFIG, 0x03);        // DLPF ~44 Hz
    writeReg(REG_GYRO_CONFIG, 0x00);   // ±250 dps
    writeReg(REG_ACCEL_CONFIG, 0x00);  // ±2 g
    accel_scale_ = 16384.0f;
    gyro_scale_  = 131.0f;

    ESP_LOGI(TAG, "MPU6050 ready at 0x%02X", addr);
    return true;
}

bool Mpu6050::probe()
{
    uint8_t who = 0;
    if (!readRegs(REG_WHO_AM_I, &who, 1)) return false;
    return who == 0x68;
}

bool Mpu6050::read(Sample& out)
{
    if (!dev_) return false;
    uint8_t b[14];
    if (!readRegs(REG_ACCEL_XOUT_H, b, sizeof(b))) return false;

    int16_t ax = (int16_t)((b[0]  << 8) | b[1]);
    int16_t ay = (int16_t)((b[2]  << 8) | b[3]);
    int16_t az = (int16_t)((b[4]  << 8) | b[5]);
    int16_t t  = (int16_t)((b[6]  << 8) | b[7]);
    int16_t gx = (int16_t)((b[8]  << 8) | b[9]);
    int16_t gy = (int16_t)((b[10] << 8) | b[11]);
    int16_t gz = (int16_t)((b[12] << 8) | b[13]);

    out.ax = ax / accel_scale_;
    out.ay = ay / accel_scale_;
    out.az = az / accel_scale_;
    out.gx = gx / gyro_scale_;
    out.gy = gy / gyro_scale_;
    out.gz = gz / gyro_scale_;
    out.temp_c = (float)t / 340.0f + 36.53f;
    return true;
}

bool Mpu6050::writeReg(uint8_t reg, uint8_t val)
{
    if (!dev_) return false;
    uint8_t buf[2] = { reg, val };
    esp_err_t err = i2c_master_transmit(dev_, buf, sizeof(buf), I2C_TIMEOUT_MS);
    if (err != ESP_OK) ESP_LOGW(TAG, "writeReg 0x%02X failed: %s", reg, esp_err_to_name(err));
    return err == ESP_OK;
}

bool Mpu6050::readRegs(uint8_t reg, uint8_t* buf, size_t len)
{
    if (!dev_) return false;
    esp_err_t err = i2c_master_transmit_receive(dev_, &reg, 1, buf, len, I2C_TIMEOUT_MS);
    if (err != ESP_OK) ESP_LOGW(TAG, "readRegs 0x%02X failed: %s", reg, esp_err_to_name(err));
    return err == ESP_OK;
}
