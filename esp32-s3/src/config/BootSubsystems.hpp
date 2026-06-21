#pragma once
#include <cstdint>

enum BootSubsystemId : uint8_t {
    BOOT_DISPLAY = 0,
    BOOT_AUDIO   = 1,
    BOOT_COMMS   = 2,
    BOOT_SUBSYSTEM_COUNT = 3
};
// Removed:
//   MOTION — servos + IMU moved to the C5 (the S3 can no longer self-test them).
//   POWER  — no battery ADC on this board (BATTERY_ADC=-1); showing a status here
//            would be fake, and the battery UI is hidden per QĐ-08.

enum BootCheckStatus : uint8_t {
    BOOT_PENDING = 0,
    BOOT_OK      = 1,
    BOOT_FAIL    = 2,
};

struct BootSubsystem {
    const char* label;
    int16_t     labelW;
};

inline constexpr BootSubsystem BOOT_SUBSYSTEMS[BOOT_SUBSYSTEM_COUNT] = {
    {"DISPLAY", 64},
    {"AUDIO",   52},
    {"COMMS",   52},
};
