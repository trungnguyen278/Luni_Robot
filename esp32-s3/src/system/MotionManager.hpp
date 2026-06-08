#pragma once
#include <cstdint>
#include <atomic>
#include <functional>
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "Pca9685.hpp"
#include "Mpu6050.hpp"
#include "config/MotionConfig.hpp"
#include "uart/UartProtocol.hpp"

// =============================================================================
// MotionManager — owns the shared I2C bus, the PCA9685 servo driver and the
// MPU6050 IMU. Servo moves run on a dedicated task so UART/network callbacks
// never block on I2C. MOTION_CMD frames from the C5 are dispatched here.
// =============================================================================
class MotionManager
{
public:
    struct Config {
        int      sda;
        int      scl;
        int      i2c_port;
        uint32_t scl_hz;
        uint8_t  pca_addr;
        uint8_t  imu_addr;
        int      pwm_freq_hz;
    };

    MotionManager() = default;
    ~MotionManager();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Enqueue a command (non-blocking). Safe to call from any task/ISR-free ctx.
    void post(uart_proto::MotionAction action, uint8_t p0 = 0, uint8_t p1 = 0);

    // Direct (blocking) primitives — prefer post() from callbacks.
    void setJoint(motion_cfg::Joint joint, uint8_t angle_deg);
    void home();
    void relax();   // detach all servos (no pulse)

    // IMU: read the latest cached sample (filled by the IMU task, not blocking).
    bool getLatestImu(Mpu6050::Sample& out);
    void getTilt(float& pitch_deg, float& roll_deg);
    bool isFallen() const { return fallen_.load(); }

    // Notified on IMU events (e.g. "fall"). Args: event name, pitch, roll (deg).
    using ImuEventCb = std::function<void(const char* event, float pitch, float roll)>;
    void setImuEventCb(ImuEventCb cb) { imu_cb_ = std::move(cb); }

    bool imuReady() const { return imu_.isReady(); }
    bool servosReady() const { return pca_.isReady(); }

private:
    struct Cmd { uint8_t action; uint8_t p0; uint8_t p1; };

    static void taskEntry(void* arg);
    void loop();
    void dispatch(const Cmd& c);

    static void imuTaskEntry(void* arg);
    void imuLoop();

    // canned demo sequences (need gait tuning per build)
    void walk(bool forward, uint8_t steps);
    void turn(bool left);
    void gesture(uint8_t id);

    uint16_t angleToPulse(const motion_cfg::ServoCal& s, uint8_t angle_deg) const;

    i2c_master_bus_handle_t bus_ = nullptr;
    Pca9685 pca_;
    Mpu6050 imu_;
    Config  cfg_{};

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t  task_  = nullptr;
    TaskHandle_t  imu_task_ = nullptr;
    std::atomic<bool> running_{false};

    // IMU latest-sample snapshot (guarded) + fall detection
    portMUX_TYPE      imu_mux_ = portMUX_INITIALIZER_UNLOCKED;
    Mpu6050::Sample   latest_{};
    float             pitch_deg_ = 0.0f;
    float             roll_deg_  = 0.0f;
    std::atomic<bool> fallen_{false};
    int               fall_count_ = 0;
    ImuEventCb        imu_cb_;
};
