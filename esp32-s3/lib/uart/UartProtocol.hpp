#pragma once
#include <cstdint>
#include <cstddef>

// =============================================================================
// UART Protocol between ESP32-C5 and ESP32-S3
// Variable-length frames for control/status messages.
// Frame: [0x55 start][type:1][len:1][payload:0-250][crc8:1]
//
// STATUS_UPDATE: C5 -> S3 (interaction, connection, system_state, emotion)
// CONTROL_CMD:   S3 -> C5 (START, END, SET_VOLUME, REBOOT, config...)
// =============================================================================

namespace uart_proto {

static constexpr uint8_t FRAME_START   = 0x55;
static constexpr size_t  MAX_PAYLOAD   = 250;
static constexpr size_t  HEADER_SIZE   = 3;   // start(1) + type(1) + len(1)
static constexpr size_t  FOOTER_SIZE   = 1;   // crc8(1)
static constexpr size_t  MAX_FRAME_SIZE = HEADER_SIZE + MAX_PAYLOAD + FOOTER_SIZE;

// Message types
enum class MsgType : uint8_t {
    STATUS_UPDATE  = 0x01,  // C5 -> S3: state snapshot
    CONTROL_CMD    = 0x02,  // S3 -> C5: command
    SYNC_DATA      = 0x03,  // C5 -> S3: weather, time, calendar (from server)
    OTA_STATUS     = 0x04,  // C5 -> S3: OTA state + progress
    LOG_ENTRY      = 0x05,  // S3 -> C5: log from S3 → forward to server
    DEVICE_CMD     = 0x06,  // C5 -> S3: command from server/app (emotion, scene, etc.)
    IMAGE_CHUNK    = 0x07,  // S3 -> C5: a JPEG camera frame, split into chunks
};

// IMAGE_CHUNK payload: [seq:2 LE][flags:1][data...]
// FIRST chunk's data begins with a header: [total_len:4 LE][width:2][height:2]
namespace image {
    static constexpr uint8_t FLAG_FIRST = 0x01;
    static constexpr uint8_t FLAG_LAST  = 0x02;
    static constexpr size_t  CHUNK_HDR  = 3;             // seq(2)+flags(1)
    static constexpr size_t  FIRST_HDR  = 8;             // total(4)+w(2)+h(2)
    static constexpr size_t  MAX_DATA   = MAX_PAYLOAD - CHUNK_HDR;  // 247
}

// Control command sub-types
enum class ControlCmd : uint8_t {
    // START payload (optional): [src:1] = state::InputSource of the trigger
    // (BUTTON / WAKEWORD). No payload = legacy sender → treat as BUTTON.
    START          = 0x01,
    END            = 0x02,
    SET_VOLUME     = 0x03,
    REBOOT         = 0x04,
    SET_BRIGHTNESS = 0x05,
    WIFI_CONFIG    = 0x10,
    WS_CONFIG      = 0x12,

    SET_EMOTION    = 0x20,
    SET_SCENE      = 0x21,
    TTS_PLAY       = 0x22,
    AUDIO_STOP     = 0x23,
    CONFIG_UPDATE  = 0x24,
    INTERACTION_MSG= 0x25,

    BOOT_DONE      = 0x30,
    BLE_PIN        = 0x31,

    // S3 -> C5: TTS playback fully drained on the speaker. C5 ends the
    // speaking session and returns the interaction state to IDLE. Without
    // this the C5 only recovers via its no-data watchdog.
    SPEAK_DONE     = 0x32,

    // Battery status. C5 -> S3 DEVICE_CMD (battery ADC moved to the C5). Payload:
    // [percent:1 (255 = no battery / hide)][power_state:1 = state::PowerState].
    SET_BATTERY    = 0x33,

    // Mic privacy mute. C5 -> S3 DEVICE_CMD. Payload: [muted:1 (1 = mute the
    // always-on mic, 0 = live)]. The mute toggle button lives on the C5 (it has
    // free GPIO; the pin-starved S3-CAM does not), but the mic itself is on the
    // S3 — so the C5 pushes the new state here. The S3 cuts the I2S input (wake
    // word + uplink go silent) and shows the status-bar mic icon (QĐ-10).
    SET_MIC_MUTE   = 0x34,

    // Motion (servo robot). Sent C5 -> S3 as a DEVICE_CMD sub-type.
    // Payload: [action:1][param:0-3]  (see MotionAction).
    MOTION_CMD     = 0x40,

    // Camera. C5 -> S3 DEVICE_CMD: request one frame to be captured + streamed
    // back to the server (as IMAGE_CHUNK frames). No payload.
    CAMERA_CAPTURE = 0x50,
};

// Motion action codes carried in a MOTION_CMD payload[0].
enum class MotionAction : uint8_t {
    HOME       = 0x00,  // all joints to neutral
    STOP       = 0x01,  // freeze / detach
    WALK_FWD   = 0x02,  // param[0] = steps (0 = continuous)
    WALK_BACK  = 0x03,
    TURN_LEFT  = 0x04,
    TURN_RIGHT = 0x05,
    GESTURE    = 0x06,  // param[0] = gesture id
    SET_JOINT  = 0x07,  // param[0] = joint index, param[1] = angle (0-180)
};

// Status payload (same as SPI). `fail_reason` (state::ConnectFailReason) was
// appended later: receivers must accept 4-byte legacy frames and default it
// to 0 (NONE) so a mixed-firmware pair degrades gracefully.
struct StatusPayload {
    uint8_t interaction;
    uint8_t connection;
    uint8_t system_state;
    uint8_t emotion;
    uint8_t fail_reason;
};

// Real CRC-8 (poly 0x07, init 0x00) — replaces the old XOR "checksum". XOR was
// commutative (byte reorders went undetected) and let same-bit-position flips in
// two different bytes cancel out; a real CRC catches both, at the same 1-byte
// footer cost. Bitwise form (no static table) so it is portable and byte-for-
// byte identical on both MCUs — both firmware trees carry a matching copy.
inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07)
                               : (uint8_t)(crc << 1);
    }
    return crc;
}

// Build a variable-length frame into `out`. Returns total frame size.
// out must have at least HEADER_SIZE + payload_len + FOOTER_SIZE bytes.
inline size_t buildFrame(uint8_t* out, MsgType type,
                         const uint8_t* payload, uint8_t payload_len)
{
    if (payload_len > MAX_PAYLOAD) return 0;

    out[0] = FRAME_START;
    out[1] = (uint8_t)type;
    out[2] = payload_len;

    if (payload_len > 0 && payload) {
        for (uint8_t i = 0; i < payload_len; ++i)
            out[HEADER_SIZE + i] = payload[i];
    }

    // CRC over type + len + payload
    out[HEADER_SIZE + payload_len] = crc8(&out[1], 2 + payload_len);

    return HEADER_SIZE + payload_len + FOOTER_SIZE;
}

// Frame parser state machine
class FrameParser {
public:
    // Feed one byte. Returns true when a complete valid frame is ready.
    bool feed(uint8_t byte) {
        switch (state_) {
        case State::WAIT_START:
            if (byte == FRAME_START) {
                state_ = State::WAIT_TYPE;
            }
            break;

        case State::WAIT_TYPE:
            type_ = byte;
            state_ = State::WAIT_LEN;
            break;

        case State::WAIT_LEN:
            len_ = byte;
            if (len_ > MAX_PAYLOAD) {
                state_ = State::WAIT_START;
                break;
            }
            idx_ = 0;
            state_ = (len_ > 0) ? State::PAYLOAD : State::WAIT_CRC;
            break;

        case State::PAYLOAD:
            payload_[idx_++] = byte;
            if (idx_ >= len_) {
                state_ = State::WAIT_CRC;
            }
            break;

        case State::WAIT_CRC: {
            state_ = State::WAIT_START;
            // Verify CRC: compute over type + len + payload
            uint8_t buf[2 + MAX_PAYLOAD];
            buf[0] = type_;
            buf[1] = len_;
            for (uint8_t i = 0; i < len_; ++i) buf[2 + i] = payload_[i];
            if (crc8(buf, 2 + len_) == byte) {
                return true;  // Frame ready
            }
            break;
        }
        }
        return false;
    }

    uint8_t       getType()       const { return type_; }
    uint8_t       getPayloadLen() const { return len_; }
    const uint8_t* getPayload()   const { return payload_; }

private:
    enum class State : uint8_t {
        WAIT_START, WAIT_TYPE, WAIT_LEN, PAYLOAD, WAIT_CRC
    };

    State   state_ = State::WAIT_START;
    uint8_t type_  = 0;
    uint8_t len_   = 0;
    uint8_t idx_   = 0;
    uint8_t payload_[MAX_PAYLOAD] = {};
};

} // namespace uart_proto
