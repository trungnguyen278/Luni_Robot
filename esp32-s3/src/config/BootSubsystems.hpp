#pragma once
#include <cstdint>

enum BootSubsystemId : uint8_t {
    BOOT_DISPLAY = 0,
    BOOT_AUDIO   = 1,
    BOOT_MOTION  = 2,   // servo + IMU (was BOOT_TOUCH; button moved to C5)
    BOOT_COMMS   = 3,
    BOOT_POWER   = 4,
    BOOT_SUBSYSTEM_COUNT = 5
};

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
    {"MOTION",  56},
    {"COMMS",   52},
    {"POWER",   52},
};
