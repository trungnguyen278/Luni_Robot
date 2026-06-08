#include "MotionManager.hpp"
#include "esp_log.h"
#include <cmath>

static const char* TAG = "MotionManager";
using namespace motion_cfg;

MotionManager::~MotionManager()
{
    stop();
    if (bus_) {
        i2c_del_master_bus(bus_);
        bus_ = nullptr;
    }
}

bool MotionManager::init(const Config& cfg)
{
    cfg_ = cfg;

    // --- Shared I2C master bus (PCA9685 + MPU6050) ---
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port   = cfg.i2c_port;
    bus_cfg.sda_io_num = (gpio_num_t)cfg.sda;
    bus_cfg.scl_io_num = (gpio_num_t)cfg.scl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;  // external 4.7k still recommended

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        bus_ = nullptr;
        return false;
    }

    bool pca_ok = pca_.init(bus_, cfg.pca_addr, cfg.scl_hz);
    if (pca_ok) {
        pca_.setPwmFreq((float)cfg.pwm_freq_hz);
        home();
    } else {
        ESP_LOGE(TAG, "PCA9685 init failed (servos disabled)");
    }

    // IMU is optional — failure is non-fatal.
    bool imu_ok = imu_.init(bus_, cfg.imu_addr, cfg.scl_hz);
    if (!imu_ok) ESP_LOGW(TAG, "MPU6050 init failed (IMU disabled)");

    queue_ = xQueueCreate(8, sizeof(Cmd));
    return pca_ok;  // servos are the must-have; IMU optional
}

void MotionManager::start()
{
    if (running_.load() || !queue_) return;
    running_ = true;
    xTaskCreatePinnedToCore(taskEntry, "Motion", 4096, this, 4, &task_, 1);
    // Low-rate IMU sampler on Core 0 (away from audio on Core 1).
    if (imu_.isReady())
        xTaskCreatePinnedToCore(imuTaskEntry, "Imu", 3072, this, 3, &imu_task_, 0);
    ESP_LOGI(TAG, "MotionManager started");
}

void MotionManager::stop()
{
    running_ = false;
}

void MotionManager::post(uart_proto::MotionAction action, uint8_t p0, uint8_t p1)
{
    if (!queue_) return;
    Cmd c{ (uint8_t)action, p0, p1 };
    xQueueSend(queue_, &c, 0);
}

void MotionManager::taskEntry(void* arg)
{
    static_cast<MotionManager*>(arg)->loop();
}

void MotionManager::loop()
{
    Cmd c;
    while (running_.load()) {
        if (xQueueReceive(queue_, &c, pdMS_TO_TICKS(200)) == pdTRUE) {
            dispatch(c);
        }
    }
    vTaskDelete(nullptr);
}

void MotionManager::dispatch(const Cmd& c)
{
    using uart_proto::MotionAction;
    switch ((MotionAction)c.action) {
    case MotionAction::HOME:       home(); break;
    case MotionAction::STOP:       relax(); break;
    case MotionAction::WALK_FWD:   walk(true,  c.p0); break;
    case MotionAction::WALK_BACK:  walk(false, c.p0); break;
    case MotionAction::TURN_LEFT:  turn(true);  break;
    case MotionAction::TURN_RIGHT: turn(false); break;
    case MotionAction::GESTURE:    gesture(c.p0); break;
    case MotionAction::SET_JOINT:
        if (c.p0 < JOINT_COUNT) setJoint((Joint)c.p0, c.p1);
        break;
    default:
        ESP_LOGW(TAG, "Unknown motion action 0x%02X", c.action);
        break;
    }
}

uint16_t MotionManager::angleToPulse(const ServoCal& s, uint8_t angle_deg) const
{
    if (angle_deg > 180) angle_deg = 180;
    uint8_t a = s.invert ? (uint8_t)(180 - angle_deg) : angle_deg;
    return (uint16_t)(s.min_us + (uint32_t)(s.max_us - s.min_us) * a / 180);
}

void MotionManager::setJoint(Joint joint, uint8_t angle_deg)
{
    if (joint >= JOINT_COUNT || !pca_.isReady()) return;
    const ServoCal& s = SERVOS[joint];
    pca_.setChannelPulseUs(s.channel, angleToPulse(s, angle_deg));
}

void MotionManager::home()
{
    if (!pca_.isReady()) return;
    for (uint8_t j = 0; j < JOINT_COUNT; ++j)
        pca_.setChannelPulseUs(SERVOS[j].channel, SERVOS[j].neutral_us);
}

void MotionManager::relax()
{
    pca_.allOff();
}

// ---- IMU low-rate sampler (Core 0) -----------------------------------------
void MotionManager::imuTaskEntry(void* arg)
{
    static_cast<MotionManager*>(arg)->imuLoop();
}

void MotionManager::imuLoop()
{
    const int ms = imu::READ_INTERVAL_MS > 0 ? imu::READ_INTERVAL_MS : 1000;
    const TickType_t period = pdMS_TO_TICKS(ms);
    Mpu6050::Sample s;
    while (running_.load()) {
        if (imu_.read(s)) {
            // Tilt from the accelerometer (deg)
            float pitch = atan2f(-s.ax, sqrtf(s.ay * s.ay + s.az * s.az)) * 57.2958f;
            float roll  = atan2f(s.ay, s.az) * 57.2958f;

            taskENTER_CRITICAL(&imu_mux_);
            latest_ = s;
            pitch_deg_ = pitch;
            roll_deg_  = roll;
            taskEXIT_CRITICAL(&imu_mux_);

            // Fall detection with hysteresis → notify once on transition.
            bool tipped = (fabsf(pitch) > imu::FALL_TILT_DEG ||
                           fabsf(roll)  > imu::FALL_TILT_DEG);
            if (tipped) {
                if (fall_count_ < imu::FALL_HOLD_SAMPLES) fall_count_++;
                if (fall_count_ >= imu::FALL_HOLD_SAMPLES && !fallen_.load()) {
                    fallen_ = true;
                    ESP_LOGW(TAG, "IMU fall: pitch=%.0f roll=%.0f", pitch, roll);
                    if (imu_cb_) imu_cb_("fall", pitch, roll);
                }
            } else {
                fall_count_ = 0;
                if (fallen_.load()) {
                    fallen_ = false;
                    if (imu_cb_) imu_cb_("upright", pitch, roll);
                }
            }
        }
        vTaskDelay(period);
    }
    vTaskDelete(nullptr);
}

bool MotionManager::getLatestImu(Mpu6050::Sample& out)
{
    if (!imu_.isReady()) return false;
    taskENTER_CRITICAL(&imu_mux_);
    out = latest_;
    taskEXIT_CRITICAL(&imu_mux_);
    return true;
}

void MotionManager::getTilt(float& pitch_deg, float& roll_deg)
{
    taskENTER_CRITICAL(&imu_mux_);
    pitch_deg = pitch_deg_;
    roll_deg  = roll_deg_;
    taskEXIT_CRITICAL(&imu_mux_);
}

// ---- Canned demo sequences (PLACEHOLDER — tune the gait per build) ----------
void MotionManager::walk(bool forward, uint8_t steps)
{
    if (!pca_.isReady()) return;
    if (steps == 0) steps = 2;
    const int amp = 25;     // degrees
    const int dwell = 180;  // ms per phase
    const int dir = forward ? 1 : -1;

    for (uint8_t i = 0; i < steps && running_.load(); ++i) {
        setJoint(ANKLE_L, 90 + amp);
        setJoint(ANKLE_R, 90 + amp);
        vTaskDelay(pdMS_TO_TICKS(dwell));
        setJoint(LEG_R, (uint8_t)(90 + dir * amp));
        vTaskDelay(pdMS_TO_TICKS(dwell));

        setJoint(ANKLE_L, 90 - amp);
        setJoint(ANKLE_R, 90 - amp);
        vTaskDelay(pdMS_TO_TICKS(dwell));
        setJoint(LEG_L, (uint8_t)(90 + dir * amp));
        vTaskDelay(pdMS_TO_TICKS(dwell));

        setJoint(LEG_L, 90);
        setJoint(LEG_R, 90);
    }
    home();
}

void MotionManager::turn(bool left)
{
    if (!pca_.isReady()) return;
    const int amp = 30, dwell = 200;
    setJoint(LEG_L, (uint8_t)(left ? 90 + amp : 90 - amp));
    setJoint(LEG_R, (uint8_t)(left ? 90 + amp : 90 - amp));
    vTaskDelay(pdMS_TO_TICKS(dwell));
    home();
}

void MotionManager::gesture(uint8_t id)
{
    if (!pca_.isReady()) return;
    switch (id) {
    case 0:  // wave right arm
        for (int i = 0; i < 3 && running_.load(); ++i) {
            setJoint(ARM_R, 150); vTaskDelay(pdMS_TO_TICKS(200));
            setJoint(ARM_R, 110); vTaskDelay(pdMS_TO_TICKS(200));
        }
        break;
    case 1:  // raise both arms (cheer)
        for (int i = 0; i < 2 && running_.load(); ++i) {
            setJoint(ARM_L, 160); setJoint(ARM_R, 160); vTaskDelay(pdMS_TO_TICKS(220));
            setJoint(ARM_L, 100); setJoint(ARM_R, 100); vTaskDelay(pdMS_TO_TICKS(220));
        }
        break;
    default:
        break;
    }
    home();
}
