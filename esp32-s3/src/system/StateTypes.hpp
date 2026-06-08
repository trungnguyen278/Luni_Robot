#pragma once
#include <cstdint>

namespace state
{

// === Connection State (mirrored from C5 via UART) ===
enum class ConnectionState : uint8_t
{
    OFFLINE,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WS_CONNECTING,
    WS_AUTHENTICATING,
    ONLINE,
    RECONNECTING,
    BLE_PROVISIONING,
    WS_ERROR,
    BLE_CONNECTED,
};

// === Interaction State (shared S3 + C5) ===
enum class InteractionState : uint8_t
{
    IDLE,
    TRIGGERED,
    LISTENING,
    PROCESSING,
    SPEAKING,
    CANCELLING,
};

// === OTA State (C5 manages, S3 displays) ===
enum class OtaState : uint8_t
{
    IDLE,
    CHECKING,
    AVAILABLE,
    DOWNLOADING,
    VERIFYING,
    FLASHING,
    REBOOTING,
    FAILED,
};

// === System State ===
enum class SystemState : uint8_t
{
    BOOTING,
    RUNNING,
    ERROR,
    MAINTENANCE,
    DEEP_SLEEP,
};

// === Power State (S3 manages battery) ===
enum class PowerState : uint8_t
{
    NORMAL,
    LOW,
    CRITICAL,
    CHARGING,
    FULL,
};

// === Emotion State (S3 renders, C5 relays from server) ===
enum class EmotionState : uint8_t
{
    NEUTRAL,
    HAPPY,
    SAD,
    ANGRY,
    CONFUSED,
    EXCITED,
    CALM,
    THINKING,
    DISGUSTED,
    NERVOUS,
    EMBARRASSED,
    CURIOUS,
    ANNOYED,
    COOL,
    SUSPICIOUS,
    DETERMINED,
};

// === Input Source ===
enum class InputSource : uint8_t
{
    BUTTON,
    SERVER_COMMAND,
    SYSTEM,
    WAKEWORD,
    UNKNOWN
};

// === Sync Data Packet (decoded from UART SYNC_DATA message) ===
struct SyncDataPacket {
    int8_t  temperature;
    uint8_t humidity;
    uint8_t condition;
    uint8_t aqi;
    uint32_t unix_time;
    int8_t  utc_offset;
    uint8_t lunar_day;
    uint8_t lunar_month;
    char    city[32];
};

} // namespace state
