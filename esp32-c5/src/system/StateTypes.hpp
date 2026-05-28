#pragma once
#include <cstdint>

namespace state
{

// === Connection State (C5 only, relayed to S3) ===
enum class ConnectionState : uint8_t
{
    OFFLINE,            // No WiFi
    WIFI_CONNECTING,    // Scanning/connecting WiFi
    WIFI_CONNECTED,     // WiFi OK, got IP
    WS_CONNECTING,      // DNS resolve + TCP connect + WS handshake
    WS_AUTHENTICATING,  // WS open, sending device_token
    ONLINE,             // Fully connected + authenticated
    RECONNECTING,       // Lost connection, auto-retry
    BLE_PROVISIONING,   // BLE config mode (no WiFi)
    WS_ERROR,           // WiFi OK but server unreachable
    BLE_CONNECTED,      // BLE provisioning successful
};

// === Connection Fail Reason ===
enum class ConnectFailReason : uint8_t
{
    NONE,

    // WiFi layer
    WIFI_NOT_FOUND,
    WIFI_AUTH_FAIL,
    WIFI_TIMEOUT,
    DHCP_FAIL,

    // Internet layer
    DNS_FAIL,
    INTERNET_CHECK_FAIL,

    // Server layer
    SERVER_UNREACHABLE,
    WS_HANDSHAKE_FAIL,
    TLS_ERROR,

    // Auth layer
    AUTH_REJECTED,
    AUTH_TIMEOUT,
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

// === Power State (S3 only, received via UART) ===
enum class PowerState : uint8_t
{
    NORMAL,
    LOW,
    CRITICAL,
    CHARGING,
    FULL,
};

// === Emotion State (S3 manages, C5 relays from server) ===
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
    UNKNOWN
};

// === State change event ===
struct StateEvent {
    enum class Type : uint8_t {
        CONNECTION, INTERACTION, OTA, SYSTEM, POWER, EMOTION
    };
    Type type;
    uint8_t old_value;
    uint8_t new_value;
    ConnectFailReason fail_reason = ConnectFailReason::NONE;
    uint32_t timestamp = 0;
};

// === Transition validation ===

inline bool isValidConnectionTransition(ConnectionState from, ConnectionState to)
{
    using CS = ConnectionState;
    if (from == to) return false;

    switch (from) {
    case CS::OFFLINE:
        return to == CS::WIFI_CONNECTING || to == CS::BLE_PROVISIONING;
    case CS::WIFI_CONNECTING:
        return to == CS::WIFI_CONNECTED || to == CS::OFFLINE
            || to == CS::BLE_PROVISIONING || to == CS::RECONNECTING;
    case CS::WIFI_CONNECTED:
        return to == CS::WS_CONNECTING || to == CS::OFFLINE;
    case CS::WS_CONNECTING:
        return to == CS::WS_AUTHENTICATING || to == CS::WS_ERROR || to == CS::RECONNECTING;
    case CS::WS_AUTHENTICATING:
        return to == CS::ONLINE || to == CS::WS_ERROR || to == CS::RECONNECTING || to == CS::BLE_PROVISIONING;
    case CS::ONLINE:
        return to == CS::RECONNECTING || to == CS::WS_ERROR;
    case CS::RECONNECTING:
        return to == CS::WS_CONNECTING || to == CS::WIFI_CONNECTING
            || to == CS::OFFLINE || to == CS::BLE_PROVISIONING;
    case CS::WS_ERROR:
        return to == CS::RECONNECTING || to == CS::OFFLINE || to == CS::WIFI_CONNECTING;
    case CS::BLE_PROVISIONING:
        return to == CS::BLE_CONNECTED || to == CS::WIFI_CONNECTING;
    case CS::BLE_CONNECTED:
        return to == CS::WIFI_CONNECTING;
    }
    return false;
}

inline bool isValidInteractionTransition(InteractionState from, InteractionState to)
{
    using IS = InteractionState;
    if (from == to) return false;

    switch (from) {
    case IS::IDLE:
        return to == IS::TRIGGERED;
    case IS::TRIGGERED:
        return to == IS::LISTENING || to == IS::IDLE;
    case IS::LISTENING:
        return to == IS::PROCESSING || to == IS::CANCELLING;
    case IS::PROCESSING:
        return to == IS::SPEAKING || to == IS::IDLE;
    case IS::SPEAKING:
        return to == IS::IDLE || to == IS::CANCELLING;
    case IS::CANCELLING:
        return to == IS::IDLE;
    }
    return false;
}

inline bool isValidOtaTransition(OtaState from, OtaState to)
{
    using OS = OtaState;
    if (from == to) return false;

    switch (from) {
    case OS::IDLE:
        return to == OS::CHECKING;
    case OS::CHECKING:
        return to == OS::AVAILABLE || to == OS::IDLE;
    case OS::AVAILABLE:
        return to == OS::DOWNLOADING || to == OS::IDLE;
    case OS::DOWNLOADING:
        return to == OS::VERIFYING || to == OS::FAILED;
    case OS::VERIFYING:
        return to == OS::FLASHING || to == OS::FAILED;
    case OS::FLASHING:
        return to == OS::REBOOTING || to == OS::FAILED;
    case OS::REBOOTING:
        return false;
    case OS::FAILED:
        return to == OS::IDLE || to == OS::CHECKING;
    }
    return false;
}

// === Retry strategy ===

struct RetryPolicy {
    uint8_t  max_retries;
    uint32_t base_delay_ms;
    uint32_t max_delay_ms;
    bool     exponential;
    bool     fallback_to_ble;
};

inline RetryPolicy getRetryPolicy(ConnectFailReason reason)
{
    using R = ConnectFailReason;
    switch (reason) {
    case R::WIFI_NOT_FOUND:     return { 3,  5000, 5000,  false, true  };
    case R::WIFI_AUTH_FAIL:     return { 0,  0,    0,     false, true  };
    case R::WIFI_TIMEOUT:       return { 3,  5000, 5000,  false, true  };
    case R::DHCP_FAIL:          return { 3,  5000, 5000,  false, true  };
    case R::DNS_FAIL:           return { 5,  10000,10000, false, true  };
    case R::INTERNET_CHECK_FAIL:return { 5,  10000,10000, false, true  };
    case R::SERVER_UNREACHABLE: return { 10, 1000, 30000, true,  false };
    case R::WS_HANDSHAKE_FAIL:  return { 5,  2000, 30000, true,  false };
    case R::TLS_ERROR:          return { 3,  5000, 5000,  false, false };
    case R::AUTH_REJECTED:      return { 0,  0,    0,     false, true  };
    case R::AUTH_TIMEOUT:       return { 3,  2000, 10000, true,  false };
    default:                    return { 3,  5000, 30000, true,  false };
    }
}

inline uint32_t calculateBackoff(const RetryPolicy& policy, uint8_t attempt)
{
    if (!policy.exponential) return policy.base_delay_ms;
    uint32_t delay = policy.base_delay_ms;
    for (uint8_t i = 0; i < attempt && delay < policy.max_delay_ms; ++i) {
        delay *= 2;
    }
    return (delay > policy.max_delay_ms) ? policy.max_delay_ms : delay;
}

} // namespace state
