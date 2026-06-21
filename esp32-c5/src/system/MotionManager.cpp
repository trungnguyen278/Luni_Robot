#include "MotionManager.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cmath>

static const char* TAG = "MotionManager";
using namespace motion_cfg;

MotionManager::~MotionManager()
{
    stop();  // joins the Motion + IMU tasks before we touch shared resources
    if (queue_) {
        vQueueDelete(queue_);
        queue_ = nullptr;
    }
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

    if (queue_) vQueueDelete(queue_);  // re-init: drop the old queue first
    queue_ = xQueueCreate(8, sizeof(Cmd));
    return pca_ok;  // servos are the must-have; IMU optional
}

void MotionManager::start()
{
    if (running_.load() || !queue_) return;
    running_ = true;
    // ESP32-C5 is single-core: pin to core 0 (the only core). Priorities 4/3 keep
    // these BELOW the WS TX (6) and SPI slave (4) so network/audio never starve.
    TaskHandle_t h = nullptr;
    xTaskCreatePinnedToCore(taskEntry, "Motion", 4096, this, 4, &h, 0);
    task_.store(h);
    // Low-rate IMU sampler (~1 Hz). Priority 3 — yields to everything important.
    if (imu_.isReady()) {
        TaskHandle_t ih = nullptr;
        xTaskCreatePinnedToCore(imuTaskEntry, "Imu", 3072, this, 3, &ih, 0);
        imu_task_.store(ih);
    }
    ESP_LOGI(TAG, "MotionManager started");
}

void MotionManager::stop()
{
    if (!running_.load()) return;
    running_ = false;
    cancel_.store(true);  // make any in-flight canned sequence bail immediately

    // Join: wait for both tasks to actually exit (they null their own handle
    // before vTaskDelete) before the dtor frees the I2C bus / queue they touch.
    // Budget > the IMU task's ~1 s sample delay; bounded so a wedged task can't
    // hang shutdown forever.
    for (int i = 0; i < 100 && (task_.load() || imu_task_.load()); ++i) {
        vTaskDelay(pdMS_TO_TICKS(20));  // up to ~2 s
    }
    if (task_.load() || imu_task_.load()) {
        ESP_LOGW(TAG, "stop(): motion task(s) did not exit in time");
    }
}

void MotionManager::post(uart_proto::MotionAction action, uint8_t p0, uint8_t p1)
{
    if (!queue_) return;

    // STOP / HOME are emergency/priority commands: signal the running sequence
    // to bail (cancel_) and flush anything queued behind so they don't have to
    // wait out a long walk before stopping. A robot moving next to a child must
    // honour STOP within a phase, not after the whole canned sequence finishes.
    using uart_proto::MotionAction;
    if (action == MotionAction::STOP || action == MotionAction::HOME) {
        cancel_.store(true);
        xQueueReset(queue_);
    }

    Cmd c{ (uint8_t)action, p0, p1 };
    if (xQueueSend(queue_, &c, 0) != pdTRUE) {
        // Queue full (a sequence is mid-flight and 8 commands already wait).
        // Without this the drop was silent and the app still claimed "sent".
        ESP_LOGW(TAG, "motion queue full — dropped action 0x%02X", (uint8_t)action);
    }
}

bool MotionManager::dwell(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
    return !aborting();
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
            // Fresh command starts un-cancelled. A STOP/HOME that pre-empted the
            // previous sequence set cancel_; that sequence has already bailed by
            // the time we get here, so clearing it now is safe.
            cancel_.store(false);
            last_motion_us_ = esp_timer_get_time();
            dispatch(c);
        } else {
            // Idle tick (no command). Drop holding torque once we've been still
            // long enough, so the servos stop buzzing/heating and draw less.
            if (servos_energized_ && pca_.isReady() &&
                (esp_timer_get_time() - last_motion_us_) >
                    (int64_t)motion_cfg::RELAX_IDLE_MS * 1000) {
                ESP_LOGD(TAG, "idle %dms — relaxing servos", motion_cfg::RELAX_IDLE_MS);
                relax();
            }
        }
    }
    task_.store(nullptr);  // signal stop()'s join that this task has exited
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

void MotionManager::markEnergized()
{
    servos_energized_ = true;
    last_motion_us_ = esp_timer_get_time();
}

void MotionManager::setJoint(Joint joint, uint8_t angle_deg)
{
    if (joint >= JOINT_COUNT || !pca_.isReady()) return;
    const ServoCal& s = SERVOS[joint];
    pca_.setChannelPulseUs(s.channel, angleToPulse(s, angle_deg));
    markEnergized();
}

void MotionManager::home()
{
    if (!pca_.isReady()) return;
    for (uint8_t j = 0; j < JOINT_COUNT; ++j)
        pca_.setChannelPulseUs(SERVOS[j].channel, SERVOS[j].neutral_us);
    markEnergized();
}

void MotionManager::relax()
{
    pca_.allOff();
    servos_energized_ = false;
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
    imu_task_.store(nullptr);  // signal stop()'s join that this task has exited
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
    if (steps > MAX_WALK_STEPS) steps = MAX_WALK_STEPS;  // bound walk duration
    const int amp = 25;        // degrees
    const int dwell_ms = 180;  // ms per phase
    const int dir = forward ? 1 : -1;

    for (uint8_t i = 0; i < steps; ++i) {
        if (aborting()) break;
        setJoint(ANKLE_L, 90 + amp);
        setJoint(ANKLE_R, 90 + amp);
        if (!dwell(dwell_ms)) break;
        setJoint(LEG_R, (uint8_t)(90 + dir * amp));
        if (!dwell(dwell_ms)) break;

        setJoint(ANKLE_L, 90 - amp);
        setJoint(ANKLE_R, 90 - amp);
        if (!dwell(dwell_ms)) break;
        setJoint(LEG_L, (uint8_t)(90 + dir * amp));
        if (!dwell(dwell_ms)) break;

        setJoint(LEG_L, 90);
        setJoint(LEG_R, 90);
    }
    // Don't yank back to home if a STOP pre-empted us — let the queued STOP
    // (relax/detach) take it from here.
    if (!aborting()) home();
}

void MotionManager::turn(bool left)
{
    if (!pca_.isReady() || aborting()) return;
    const int amp = 30, dwell_ms = 200;
    setJoint(LEG_L, (uint8_t)(left ? 90 + amp : 90 - amp));
    setJoint(LEG_R, (uint8_t)(left ? 90 + amp : 90 - amp));
    if (!dwell(dwell_ms)) return;
    if (!aborting()) home();
}

void MotionManager::gesture(uint8_t id)
{
    if (!pca_.isReady() || aborting()) return;
    switch (id) {
    case 0:  // wave right arm
        for (int i = 0; i < 3 && !aborting(); ++i) {
            setJoint(ARM_R, 150); if (!dwell(200)) break;
            setJoint(ARM_R, 110); if (!dwell(200)) break;
        }
        break;
    case 1:  // raise both arms (cheer)
        for (int i = 0; i < 2 && !aborting(); ++i) {
            setJoint(ARM_L, 160); setJoint(ARM_R, 160); if (!dwell(220)) break;
            setJoint(ARM_L, 100); setJoint(ARM_R, 100); if (!dwell(220)) break;
        }
        break;
    default:
        break;
    }
    if (!aborting()) home();
}
