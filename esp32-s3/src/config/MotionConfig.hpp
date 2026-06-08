#pragma once
#include <cstdint>

// =============================================================================
// Luni Motion config — 6 servos on the PCA9685 (EMO/Otto-style robot).
//
// Mỗi chân 2 servo: trục Z xoay chân (LEG_*) + cổ chân lăn ngang/roll (ANKLE_*);
// cộng 2 tay (ARM_*). Tổng 6 servo (CH0..CH5). Hiệu chỉnh pulse/invert phải
// TUNE theo cơ khí thật. Tham số IMU cũng để ở đây cho dễ chỉnh.
// =============================================================================
namespace motion_cfg {

enum Joint : uint8_t {
    LEG_L   = 0,   // chân trái — trục Z (xoay chân, để bước/xoay)
    LEG_R   = 1,   // chân phải — trục Z
    ANKLE_L = 2,   // cổ chân trái — lăn ngang (roll, dồn trọng tâm)
    ANKLE_R = 3,   // cổ chân phải — lăn ngang
    ARM_L   = 4,   // tay trái
    ARM_R   = 5,   // tay phải
    JOINT_COUNT = 6
};

struct ServoCal {
    uint8_t  channel;      // PCA9685 channel
    uint16_t min_us;       // pulse at logical angle 0°
    uint16_t max_us;       // pulse at logical angle 180°
    uint16_t neutral_us;   // "home" pulse (usually ~90°)
    bool     invert;       // reverse direction (mirror left/right servos)
};

// Default SG90/MG90-style calibration. TUNE per build.
inline constexpr ServoCal SERVOS[JOINT_COUNT] = {
    /* LEG_L   */ { 0, 500, 2500, 1500, false },
    /* LEG_R   */ { 1, 500, 2500, 1500, true  },
    /* ANKLE_L */ { 2, 500, 2500, 1500, false },
    /* ANKLE_R */ { 3, 500, 2500, 1500, true  },
    /* ARM_L   */ { 4, 500, 2500, 1500, false },
    /* ARM_R   */ { 5, 500, 2500, 1500, true  },
};

// --- IMU (MPU6050) — đọc rất nhẹ, chỉ để báo sự kiện lên server (không balance) ---
// Dùng chu kỳ ms (thay vì Hz) để hỗ trợ tần số < 1Hz nếu cần.
namespace imu {
    static constexpr int   READ_INTERVAL_MS = 1000;  // 1Hz (đặt 2000 = 0.5Hz, 5000 = 0.2Hz...)
    static constexpr float FALL_TILT_DEG     = 60.0f; // |pitch|/|roll| vượt → nghi ngã
    static constexpr int   FALL_HOLD_SAMPLES = 2;     // giữ N mẫu liên tiếp mới báo (chống nhiễu)
}

} // namespace motion_cfg
