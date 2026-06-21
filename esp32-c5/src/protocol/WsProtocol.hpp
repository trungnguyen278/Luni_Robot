#pragma once
#include <cstdint>
#include <cstddef>
#include "cJSON.h"

namespace ws {

// Text frame message types — MUST match JSON "type" strings
enum class MsgType : uint8_t {
    // Handshake
    AUTH,
    AUTH_RESULT,

    // Device → Server
    DEVICE_INFO,
    STATE_UPDATE,
    HEARTBEAT,
    LOG,
    OTA_PROGRESS,
    ERROR_REPORT,

    // Server → Device
    SET_VOLUME,
    SET_BRIGHTNESS,
    SET_EMOTION,
    SET_SCENE,
    REBOOT,
    OTA_AVAILABLE,
    SYNC_DATA,
    TTS_PLAY,
    AUDIO_STOP,
    CONFIG_UPDATE,
    INTERACTION_MSG,
    ACK,
    MOTION,           // servo robot movement → relayed to S3 as MOTION_CMD
    CAMERA_CAPTURE,   // request one camera frame → relayed to S3

    UNKNOWN = 0xFF,
};

MsgType msgTypeFromString(const char* str);
const char* msgTypeToString(MsgType type);

// Binary frame header (5 bytes)
struct AudioFrameHeader {
    uint8_t  direction;     // 0xAA = uplink (mic), 0xAB = downlink (speaker)
    uint16_t sequence;
    uint16_t length;
};

static constexpr uint8_t AUDIO_UPLINK   = 0xAA;
static constexpr uint8_t AUDIO_DOWNLINK = 0xAB;
// Camera image uplink (device → server): first byte tag, then raw JPEG bytes.
// One whole-image frame — only usable for images small enough for the C5 to hold.
static constexpr uint8_t IMAGE_UPLINK   = 0xAC;
// Camera image CHUNK uplink (device → server): the C5 streams a JPEG one chunk
// per WS frame so it never buffers the whole image (no PSRAM). The server
// reassembles. Frame: [0xAD][seq:2 LE][flags:1][data] — mirrors the UART
// IMAGE_CHUNK payload (FIRST data begins [total:4 LE][w:2][h:2], then JPEG).
static constexpr uint8_t IMAGE_CHUNK_UPLINK = 0xAD;

// JSON message builder helpers
cJSON* createMessage(MsgType type, const char* id = nullptr);
cJSON* createAuthMessage(const char* mac, const char* device_token,
                         const char* fw_version, const char* model);
cJSON* createDeviceInfo(const char* mac, const char* fw_version,
                        const char* model, const char* name);
cJSON* createStateUpdate(const char* category, uint8_t old_val, uint8_t new_val);
// String-valued variant: emotion carries a 45-key string, not the lossy 16-value
// C5 enum, so the app/web get the real emotion in `new` (same shape as the
// numeric form). `old` is omitted — no consumer reads it.
cJSON* createStateUpdateStr(const char* category, const char* new_val);
cJSON* createHeartbeat(uint32_t uptime, uint32_t free_heap, int8_t rssi);
cJSON* createLog(const char* level, const char* tag, const char* message);
cJSON* createOtaProgress(uint8_t percent, const char* phase);
cJSON* createErrorReport(uint16_t code, const char* message, const char* severity);

// Message parser
struct ParsedMessage {
    MsgType type;
    char id[37];        // UUID
    cJSON* payload;     // Caller must cJSON_Delete the root
    cJSON* root;        // Root JSON object — caller must cJSON_Delete
    bool valid;
};

ParsedMessage parseMessage(const char* json, size_t len);

// Serialize a cJSON message to string. Caller must free with cJSON_free().
char* serializeMessage(cJSON* msg);

} // namespace ws
