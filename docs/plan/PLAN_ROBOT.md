# Luni Robot Firmware — Refactoring Plan

> Platform: ESP32-S3 + ESP32-C5 | Framework: ESP-IDF + PlatformIO
> Server: FastAPI (self-hosted, Docker) expose qua Cloudflare Tunnel (free)
> Mục tiêu: Bỏ MQTT, chuyển sang WS-only + HTTP OTA, thống nhất state machine

---

## 1. Tổng quan thay đổi

### Bỏ hoàn toàn
- `MqttClient.cpp/hpp` — thay bằng WebSocket-only
- `MQTTConfig.hpp` — thay bằng protocol mới
- Topic-based message routing — thay bằng JSON message type routing
- MQTT OTA (binary chunks qua MQTT) — thay bằng HTTP chunked download

### Giữ nguyên
- **Inter-MCU protocol** (SPI + UART) — đã ổn định, không cần đổi
- **UI system** (GfxEngine, SceneManager, VariantRegistry, 227 variants) — không đổi
- **Audio pipeline** (Opus 4-task) — không đổi
- **BLE provisioning** — giữ, bổ sung thêm fields
- **Power management** — không đổi

### Refactor lớn
- `NetworkManager` — viết lại, bỏ MQTT orchestration
- `WebSocketClient` — nâng cấp, thêm binary frame support
- State machine — thống nhất, thêm connection lifecycle
- OTA — chuyển sang HTTP client
- Data sync — nhận sync_data từ server qua WS

---

## 2. Architecture mới (ESP32-C5)

### 2.1 File structure sau refactor

```
esp32-c5/
├── src/
│   ├── main.cpp
│   ├── AppController.cpp/hpp           # Giữ, update event handling
│   ├── config/
│   │   ├── DeviceProfile.cpp/hpp       # Giữ, update fields
│   │   ├── PinConfig.hpp               # Giữ nguyên
│   │   └── ServerConfig.hpp            # MỚI: server URLs, timeouts
│   ├── system/
│   │   ├── StateManager.cpp/hpp        # REFACTOR: thống nhất state machine
│   │   ├── StateTypes.hpp              # REFACTOR: thêm ConnectionState, OtaState
│   │   ├── NetworkManager.cpp/hpp      # VIẾT LẠI: WS-only + HTTP OTA
│   │   ├── BluetoothService.cpp/hpp    # UPDATE: MAC-based identity, bỏ device_token
│   │   ├── DataSyncManager.cpp/hpp     # MỚI: quản lý sync_data
│   │   ├── OtaManager.cpp/hpp          # MỚI: HTTP-based OTA
│   │   ├── LogManager.cpp/hpp          # MỚI: structured logging → server
│   │   ├── SpiBridge.cpp/hpp           # Giữ nguyên
│   │   ├── UartBridge.cpp/hpp          # Giữ nguyên
│   │   └── CliConsole.cpp/hpp          # Giữ nguyên
│   └── protocol/
│       ├── WsProtocol.hpp              # MỚI: WS message types & serialization
│       └── WsMessageHandler.cpp/hpp    # MỚI: incoming message dispatcher
│
├── lib/
│   ├── network/
│   │   ├── WifiService.cpp/hpp         # Giữ, minor updates
│   │   ├── WebSocketClient.cpp/hpp     # REFACTOR: text + binary frame support
│   │   ├── HttpClient.cpp/hpp          # MỚI: cho OTA download
│   │   └── web_page.hpp               # Giữ nguyên
│   ├── spi/
│   │   └── SpiProtocol.hpp            # Giữ nguyên
│   └── uart/
│       └── UartProtocol.hpp           # Giữ nguyên

# XOÁ:
#   ├── lib/network/MqttClient.cpp/hpp    ← XOÁ
#   └── include/system/MQTTConfig.hpp     ← XOÁ
```

### 2.2 File changes cho ESP32-S3

```
esp32-s3/
├── src/
│   ├── system/
│   │   ├── StateManager.cpp/hpp        # SYNC: match C5's unified state types
│   │   ├── StateTypes.hpp              # SYNC: match C5
│   │   ├── DisplayManager.cpp/hpp      # UPDATE: handle sync_data cho scenes
│   │   └── SpiBridge.cpp/hpp           # UPDATE: thêm SYNC_DATA message type
│   └── ui/
│       └── SceneManager.cpp/hpp        # UPDATE: consume sync_data (weather, time, calendar)
```

## 3. State Machine thống nhất

### 3.1 StateTypes.hpp (mới)

```cpp
// === Connection State (C5 only, relayed to S3) ===
enum class ConnectionState : uint8_t {
    OFFLINE,            // No WiFi
    WIFI_CONNECTING,    // Scanning/connecting WiFi
    WIFI_CONNECTED,     // WiFi OK, got IP
    WS_CONNECTING,      // DNS resolve + TCP connect + WS handshake
    WS_AUTHENTICATING,  // WS open, sending MAC identity
    ONLINE,             // Fully connected + authenticated
    RECONNECTING,       // Lost connection, auto-retry
    BLE_PROVISIONING,   // BLE config mode (no WiFi)
};

// === Connection Fail Reason (phân biệt nguyên nhân lỗi) ===
// Dùng kèm RECONNECTING state → S3 hiển thị đúng thông báo + quyết định retry strategy
enum class ConnectFailReason : uint8_t {
    NONE,                // No error

    // WiFi layer
    WIFI_NOT_FOUND,      // SSID không tìm thấy
    WIFI_AUTH_FAIL,      // Sai mật khẩu WiFi
    WIFI_TIMEOUT,        // Association timeout
    DHCP_FAIL,           // WiFi connected nhưng không nhận được IP

    // Internet layer
    DNS_FAIL,            // Không resolve được hostname server → không có internet
    INTERNET_CHECK_FAIL, // Resolve OK domain khác nhưng fail ping → captive portal / firewall

    // Server layer
    SERVER_UNREACHABLE,  // DNS OK, TCP connect refused/timeout → server down
    WS_HANDSHAKE_FAIL,   // TCP OK, WS upgrade bị reject → server lỗi
    TLS_ERROR,           // Certificate verify fail

    // Auth layer
    AUTH_REJECTED,       // MAC không được đăng ký trên server (WS close 4001)
    AUTH_TIMEOUT,        // Server không phản hồi auth trong 5s
};

// === Interaction State (shared S3 + C5) ===
enum class InteractionState : uint8_t {
    IDLE,               // Waiting for trigger
    TRIGGERED,          // Button/wake-word detected
    LISTENING,          // Mic active, streaming audio
    PROCESSING,         // Server processing (STT+LLM)
    SPEAKING,           // Playing TTS response
    CANCELLING,         // User cancelled mid-interaction
};

// === OTA State (C5 manages, S3 displays) ===
enum class OtaState : uint8_t {
    IDLE,               // No OTA activity
    CHECKING,           // Querying server for update
    AVAILABLE,          // Update available, waiting for user/auto
    DOWNLOADING,        // HTTP download in progress
    VERIFYING,          // SHA256 verification
    FLASHING,           // Writing to flash partition
    REBOOTING,          // Reboot countdown
    FAILED,             // Error occurred
};

// === System State ===
enum class SystemState : uint8_t {
    BOOTING,
    RUNNING,
    ERROR,
    MAINTENANCE,        // OTA or config update in progress
    DEEP_SLEEP,
};

// === Power State (S3 only) ===
enum class PowerState : uint8_t {
    NORMAL,             // > 20%
    LOW,                // 10-20%
    CRITICAL,           // < 10%
    CHARGING,           // TP4056 charging
    FULL,               // Charge complete
};

// === Emotion State (S3 manages, C5 relays from server) ===
enum class EmotionState : uint8_t {
    NEUTRAL, HAPPY, SAD, ANGRY, CONFUSED, EXCITED,
    CALM, THINKING, DISGUSTED, NERVOUS, EMBARRASSED,
    CURIOUS, ANNOYED, COOL, SUSPICIOUS, DETERMINED,
    // ... match VariantRegistry categories
};

// === State change event ===
struct StateEvent {
    enum class Type : uint8_t {
        CONNECTION, INTERACTION, OTA, SYSTEM, POWER, EMOTION
    };
    Type type;
    uint8_t old_value;
    uint8_t new_value;
    ConnectFailReason fail_reason;  // only meaningful when type==CONNECTION
    uint32_t timestamp;             // millis()
};
```

### 3.2 State Transition Rules

```cpp
// Connection transitions (valid only)
// Mỗi transition khi fail sẽ set ConnectFailReason → S3 hiển thị + quyết định retry
const StateTransition connectionRules[] = {
    // --- Boot / WiFi ---
    { OFFLINE,          WIFI_CONNECTING },
    { OFFLINE,          BLE_PROVISIONING },   // Không có credentials
    { WIFI_CONNECTING,  WIFI_CONNECTED },     // WiFi OK + got IP
    { WIFI_CONNECTING,  OFFLINE },            // → WIFI_NOT_FOUND | WIFI_AUTH_FAIL | DHCP_FAIL
    { WIFI_CONNECTING,  BLE_PROVISIONING },   // → WIFI_TIMEOUT (lâu quá → cho pair lại)

    // --- Internet + Server ---
    { WIFI_CONNECTED,   WS_CONNECTING },
    { WIFI_CONNECTED,   OFFLINE },            // WiFi lost ngay sau connect
    { WS_CONNECTING,    WS_AUTHENTICATING },  // TCP + WS handshake OK
    { WS_CONNECTING,    RECONNECTING },       // → DNS_FAIL | SERVER_UNREACHABLE | WS_HANDSHAKE_FAIL | TLS_ERROR

    // --- Auth ---
    { WS_AUTHENTICATING,ONLINE },             // Auth OK
    { WS_AUTHENTICATING,RECONNECTING },       // → AUTH_TIMEOUT (server lag)
    { WS_AUTHENTICATING,BLE_PROVISIONING },   // → AUTH_REJECTED 4001 (token sai → pair lại)

    // --- Online → Disconnect ---
    { ONLINE,           RECONNECTING },       // Connection lost

    // --- Reconnect logic (xem fail_reason để quyết định) ---
    { RECONNECTING,     WS_CONNECTING },      // WiFi vẫn OK → retry server
    { RECONNECTING,     WIFI_CONNECTING },    // WiFi cũng mất → retry từ đầu
    { RECONNECTING,     OFFLINE },            // Max retries exceeded
    { RECONNECTING,     BLE_PROVISIONING },   // DNS_FAIL liên tục → có thể sai WiFi → pair lại

    // --- BLE ---
    { BLE_PROVISIONING, WIFI_CONNECTING },    // Nhận credentials mới
};

// Retry strategy theo ConnectFailReason:
//
// | Reason             | Retry? | Backoff    | Fallback sau N lần  |
// |--------------------|--------|------------|---------------------|
// | WIFI_NOT_FOUND     | 3x     | 5s fixed   | BLE_PROVISIONING    |
// | WIFI_AUTH_FAIL     | No     | —          | BLE_PROVISIONING    |
// | DHCP_FAIL          | 3x     | 5s fixed   | BLE_PROVISIONING    |
// | DNS_FAIL           | 5x     | 10s fixed  | BLE_PROVISIONING    |
// | SERVER_UNREACHABLE | 10x    | exp 1→30s  | OFFLINE (wait)      |
// | WS_HANDSHAKE_FAIL  | 5x     | exp 2→30s  | OFFLINE             |
// | TLS_ERROR          | 3x     | 5s fixed   | OFFLINE             |
// | AUTH_REJECTED      | No     | —          | BLE_PROVISIONING    |
// | AUTH_TIMEOUT       | 3x     | exp 2→10s  | RECONNECTING        |

// Interaction transitions
const StateTransition interactionRules[] = {
    { IDLE,        TRIGGERED },
    { TRIGGERED,   LISTENING },
    { TRIGGERED,   IDLE },          // Cancel before mic starts
    { LISTENING,   PROCESSING },
    { LISTENING,   CANCELLING },    // User cancel
    { PROCESSING,  SPEAKING },
    { PROCESSING,  IDLE },          // Server error or empty response
    { SPEAKING,    IDLE },          // Audio complete
    { SPEAKING,    CANCELLING },    // User interrupts
    { CANCELLING,  IDLE },
};

// OTA transitions
const StateTransition otaRules[] = {
    { IDLE,         CHECKING },
    { CHECKING,     AVAILABLE },
    { CHECKING,     IDLE },         // No update
    { AVAILABLE,    DOWNLOADING },
    { AVAILABLE,    IDLE },         // User declined
    { DOWNLOADING,  VERIFYING },
    { DOWNLOADING,  FAILED },
    { VERIFYING,    FLASHING },
    { VERIFYING,    FAILED },       // Bad hash
    { FLASHING,     REBOOTING },
    { FLASHING,     FAILED },
    { FAILED,       IDLE },         // Reset after error
    { FAILED,       CHECKING },     // Retry
};
```

## 4. WebSocket Protocol Implementation

### 4.1 WsProtocol.hpp

```cpp
#pragma once
#include <cstdint>
#include "cJSON.h"

namespace ws {

// Text frame message types — MUST match JSON "type" strings (see mapping below)
enum class MsgType : uint8_t {
    // === Handshake ===
    AUTH,               // Device → Server: first message (mac as device identity)
    AUTH_RESULT,        // Server → Device: { status: "ok"|"fail" }

    // === Device → Server ===
    DEVICE_INFO,        // Device info after auth OK
    STATE_UPDATE,       // State change notification
    HEARTBEAT,          // Periodic health check (30s)
    LOG,                // Log entry
    OTA_PROGRESS,       // OTA status update
    ERROR_REPORT,       // Error notification

    // === Server → Device ===
    SET_VOLUME,         // { value: 0-100 }
    SET_BRIGHTNESS,     // { value: 0-100 }
    SET_EMOTION,        // { emotion, variant }
    SET_SCENE,          // { scene, data }
    REBOOT,             // {}
    OTA_AVAILABLE,      // { version, url, sha256, size }
    SYNC_DATA,          // Weather/time/calendar data
    TTS_PLAY,           // { text }
    AUDIO_STOP,         // {}
    CONFIG_UPDATE,      // { key, value }
    INTERACTION_MSG,    // { from, text, source }
    ACK,                // { ref_id }

    UNKNOWN = 0xFF,
};

// JSON "type" string ↔ MsgType mapping
// Dùng cho serialize/deserialize WS text frames
static constexpr struct { const char* str; MsgType type; } MSG_TYPE_MAP[] = {
    // Handshake
    { "auth",              MsgType::AUTH },
    { "auth_result",       MsgType::AUTH_RESULT },
    // Device → Server
    { "device_info",       MsgType::DEVICE_INFO },
    { "state_update",      MsgType::STATE_UPDATE },
    { "heartbeat",         MsgType::HEARTBEAT },
    { "log",               MsgType::LOG },
    { "ota_progress",      MsgType::OTA_PROGRESS },
    { "error",             MsgType::ERROR_REPORT },
    // Server → Device
    { "set_volume",        MsgType::SET_VOLUME },
    { "set_brightness",    MsgType::SET_BRIGHTNESS },
    { "set_emotion",       MsgType::SET_EMOTION },
    { "set_scene",         MsgType::SET_SCENE },
    { "reboot",            MsgType::REBOOT },
    { "ota_available",     MsgType::OTA_AVAILABLE },
    { "sync_data",         MsgType::SYNC_DATA },
    { "tts_play",          MsgType::TTS_PLAY },
    { "audio_stop",        MsgType::AUDIO_STOP },
    { "config_update",     MsgType::CONFIG_UPDATE },
    { "interaction_msg",   MsgType::INTERACTION_MSG },
    { "ack",               MsgType::ACK },
};

MsgType msgTypeFromString(const char* str);
const char* msgTypeToString(MsgType type);

// Binary frame header (5 bytes)
struct AudioFrameHeader {
    uint8_t  direction;     // 0xAA = uplink (mic), 0xAB = downlink (speaker)
    uint16_t sequence;      // Packet sequence number (LE)
    uint16_t length;        // Payload length (LE)
};

// JSON message builder helpers
cJSON* createMessage(MsgType type, const char* id = nullptr);
cJSON* createStateUpdate(const char* category, uint8_t old_val, uint8_t new_val);
cJSON* createHeartbeat(uint32_t uptime, uint32_t free_heap, int8_t rssi);
cJSON* createLog(const char* level, const char* tag, const char* message);
cJSON* createOtaProgress(uint8_t percent, const char* phase);
cJSON* createDeviceInfo(const char* mac, const char* fw_version,
                        const char* model, const char* name);

// Message parser
struct ParsedMessage {
    MsgType type;
    char id[37];        // UUID
    cJSON* payload;     // Caller must cJSON_Delete
    bool valid;
};

ParsedMessage parseMessage(const char* json, size_t len);

} // namespace ws
```

### 4.2 WebSocketClient refactor

```cpp
// Key changes from current implementation:

class WebSocketClient {
public:
    // Connect with MAC-based identification
    esp_err_t connect(const char* url);
    void disconnect();
    bool isConnected() const;

    // Text frames (JSON commands/status)
    esp_err_t sendText(const char* json, size_t len);
    esp_err_t sendMessage(cJSON* msg);  // Auto-serialize + delete

    // Binary frames (audio)
    esp_err_t sendBinary(const uint8_t* data, size_t len);
    esp_err_t sendAudioFrame(uint16_t seq, const uint8_t* opus_data, size_t opus_len);

    // Callbacks
    using TextCallback = std::function<void(const char* data, size_t len)>;
    using BinaryCallback = std::function<void(const uint8_t* data, size_t len)>;
    using StateCallback = std::function<void(bool connected)>;

    void onText(TextCallback cb);
    void onBinary(BinaryCallback cb);
    void onStateChange(StateCallback cb);

private:
    // Authentication flow (sau WS open):
    // 1. Gửi: { type: "auth", payload: { mac, fw_version, model } }
    //    MAC = BLE MAC (ESP_MAC_BT), là device identity vĩnh viễn
    // 2. Chờ: { type: "auth_result", payload: { status: "ok" } } — timeout 5s
    // 3. OK → transition ONLINE, gửi device_info
    //    Fail → server close 4001 (MAC chưa đăng ký) → BLE_PROVISIONING
    esp_err_t authenticate();

    // Auto-reconnect với exponential backoff
    void scheduleReconnect();
    uint32_t reconnectDelay_;       // 1s → 2s → 4s → 8s → 16s → 30s max
    uint8_t  reconnectAttempts_;    // Max 10, sau đó → OFFLINE

    // TX buffer management (giữ từ code hiện tại)
    StreamBufferHandle_t txBuffer_;
    TaskHandle_t txTask_;
};
```

## 5. NetworkManager viết lại

```cpp
class NetworkManager {
public:
    void init(StateManager& sm);
    void start();

    // Commands from AppController / SPI bridge
    void sendStateUpdate(StateEvent event);
    void sendAudioUplink(const uint8_t* opus_data, size_t len);
    void sendLog(const char* level, const char* tag, const char* msg);

    // OTA trigger
    void checkOtaUpdate();

private:
    StateManager* stateManager_;
    WifiService wifi_;
    WebSocketClient ws_;
    OtaManager ota_;
    DataSyncManager sync_;
    LogManager log_;

    // Connection lifecycle task
    static void connectionTask(void* arg);
    void runConnectionStateMachine();

    // Message handler (incoming WS text frames)
    WsMessageHandler msgHandler_;

    // Heartbeat timer
    esp_timer_handle_t heartbeatTimer_;
    static void heartbeatCallback(void* arg);
    uint32_t heartbeatInterval_ = 30000;  // 30s

    // State
    ConnectionState connState_ = ConnectionState::OFFLINE;
    ConnectFailReason lastFailReason_ = ConnectFailReason::NONE;
    bool wifiCredentialsExist_ = false;

    // Fail reason → retry strategy
    uint8_t maxRetriesForReason(ConnectFailReason reason) const;
    uint32_t backoffForReason(ConnectFailReason reason, uint8_t attempt) const;
    bool shouldFallbackToBle(ConnectFailReason reason, uint8_t attempts) const;
};
```

### Connection State Machine Implementation

```cpp
void NetworkManager::runConnectionStateMachine() {
    while (true) {
        switch (connState_) {
            case ConnectionState::OFFLINE:
                if (wifiCredentialsExist_) {
                    transition(ConnectionState::WIFI_CONNECTING);
                    wifi_.connect();
                } else {
                    transition(ConnectionState::BLE_PROVISIONING);
                }
                break;

            case ConnectionState::WIFI_CONNECTING: {
                auto result = wifi_.waitConnect(30000);
                if (result == WifiResult::OK) {
                    transition(ConnectionState::WIFI_CONNECTED);
                } else {
                    // Map WiFi error → ConnectFailReason
                    switch (result) {
                        case WifiResult::SSID_NOT_FOUND:
                            lastFailReason_ = ConnectFailReason::WIFI_NOT_FOUND; break;
                        case WifiResult::AUTH_FAIL:
                            lastFailReason_ = ConnectFailReason::WIFI_AUTH_FAIL; break;
                        case WifiResult::NO_IP:
                            lastFailReason_ = ConnectFailReason::DHCP_FAIL; break;
                        default:
                            lastFailReason_ = ConnectFailReason::WIFI_TIMEOUT; break;
                    }
                    // AUTH_FAIL → thẳng BLE (sai mật khẩu, retry không ý nghĩa)
                    if (lastFailReason_ == ConnectFailReason::WIFI_AUTH_FAIL) {
                        transition(ConnectionState::BLE_PROVISIONING);
                    } else {
                        transition(ConnectionState::RECONNECTING);
                    }
                }
                break;
            }

            case ConnectionState::WIFI_CONNECTED:
                transition(ConnectionState::WS_CONNECTING);
                ws_.connect(serverUrl_, deviceToken_);
                break;

            case ConnectionState::WS_CONNECTING: {
                // WS connect flow: DNS resolve → TCP connect → WS upgrade
                // Mỗi bước fail → set fail reason khác nhau
                auto result = waitForWsOpen(10000);
                if (result == WsConnectResult::OK) {
                    // WS handshake OK → authenticate
                } else {
                    switch (result) {
                        case WsConnectResult::DNS_FAIL:
                            lastFailReason_ = ConnectFailReason::DNS_FAIL; break;
                        case WsConnectResult::TCP_FAIL:
                            lastFailReason_ = ConnectFailReason::SERVER_UNREACHABLE; break;
                        case WsConnectResult::TLS_FAIL:
                            lastFailReason_ = ConnectFailReason::TLS_ERROR; break;
                        default:
                            lastFailReason_ = ConnectFailReason::WS_HANDSHAKE_FAIL; break;
                    }
                    transition(ConnectionState::RECONNECTING);
                }
                break;
            }

            case ConnectionState::WS_AUTHENTICATING:
                // WebSocketClient::authenticate() flow:
                // 1. Send { type: "auth", payload: { mac, fw_version, model } }
                //    MAC (BLE) = device identity, survive NVS reset
                // 2. Wait auth_result (timeout 5s)
                // 3. status:"ok" → ONLINE
                // 4. status:"fail" or timeout → RECONNECTING
                // 5. WS close 4001 (MAC chưa đăng ký) → BLE_PROVISIONING
                break;

            case ConnectionState::ONLINE:
                // Gửi device_info
                sendDeviceInfo();
                // Start heartbeat timer
                startHeartbeat();
                // Wait for disconnect event
                waitForDisconnect();
                break;

            case ConnectionState::RECONNECTING: {
                stopHeartbeat();
                uint8_t maxRetries = maxRetriesForReason(lastFailReason_);
                uint32_t delay = backoffForReason(lastFailReason_, reconnectAttempts_);

                if (reconnectAttempts_ < maxRetries) {
                    vTaskDelay(pdMS_TO_TICKS(delay));
                    reconnectAttempts_++;

                    if (!wifi_.isConnected()) {
                        transition(ConnectionState::WIFI_CONNECTING);
                    } else {
                        transition(ConnectionState::WS_CONNECTING);
                    }
                } else if (shouldFallbackToBle(lastFailReason_, reconnectAttempts_)) {
                    // DNS_FAIL / WIFI_NOT_FOUND liên tục → credentials sai → pair lại
                    reconnectAttempts_ = 0;
                    transition(ConnectionState::BLE_PROVISIONING);
                } else {
                    // SERVER_UNREACHABLE / TLS / WS fail → đợi rồi thử lại từ đầu
                    reconnectAttempts_ = 0;
                    transition(ConnectionState::OFFLINE);
                }
                break;
            }

            case ConnectionState::BLE_PROVISIONING:
                // BLE task handles credentials
                // On credentials received → WIFI_CONNECTING
                waitForBleCredentials();
                transition(ConnectionState::WIFI_CONNECTING);
                break;
        }
    }
}
```

## 6. OTA Manager (HTTP-based)

```cpp
class OtaManager {
public:
    void init(StateManager& sm);

    // Trigger check (called from WS command or periodic)
    void checkUpdate(const char* current_version, const char* model);

    // Start download (called after user confirmation or auto-update policy)
    void startDownload(const char* url, const char* sha256, size_t size);

    // Cancel ongoing OTA
    void cancel();

    // Get progress
    OtaState getState() const;
    uint8_t getProgress() const;

private:
    StateManager* sm_;
    OtaState state_ = OtaState::IDLE;
    uint8_t progress_ = 0;

    // HTTP download task
    static void downloadTask(void* arg);

    struct OtaInfo {
        char url[256];
        char sha256[65];
        size_t size;
    } pending_;

    // ESP OTA handle
    esp_ota_handle_t otaHandle_;
    const esp_partition_t* updatePartition_;

    // Download flow:
    // 1. HTTP GET url (chunked transfer, 4KB buffer)
    // 2. For each chunk:
    //    a. esp_ota_write(handle, chunk, len)
    //    b. Update SHA256 context
    //    c. Update progress % → notify StateManager
    // 3. esp_ota_end(handle)
    // 4. Verify SHA256
    // 5. esp_ota_set_boot_partition(updatePartition)
    // 6. Reboot countdown (3s)
};
```

## 7. Data Sync Manager

```cpp
class DataSyncManager {
public:
    void init();

    // Called when server pushes sync_data via WS
    void handleSyncData(cJSON* payload);

    // Getters for SceneManager (via UART relay to S3)
    const SyncData& getData() const { return data_; }
    bool hasValidData() const { return dataValid_; }

    // Relay to S3 via UART (control channel, not SPI)
    void relayToS3(UartBridge& uart);

private:
    struct SyncData {
        // Time
        int64_t unixTime;
        char timezone[32];
        int8_t utcOffset;

        // Weather
        int8_t  temperature;
        int8_t  feelsLike;
        uint8_t humidity;
        char    condition[24];     // "partly_cloudy", "rain", etc.
        uint8_t aqi;

        // Calendar
        struct {
            uint8_t lunarDay;
            uint8_t lunarMonth;
            char    lunarYear[12]; // "Ất Tỵ"
        } lunar;

        // Location
        char city[32];
    };

    SyncData data_;
    bool dataValid_ = false;
    uint32_t lastUpdate_ = 0;
};

// UART message for sync_data relay to S3
// MsgType::SYNC_DATA (0x03), variable-length payload:
// [temp:1] [humidity:1] [condition:1(enum)] [aqi:1]
// [unix_time:4 LE] [utc_offset:1] [lunar_day:1] [lunar_month:1]
// [city: null-terminated string]
// Total ~40 bytes — fits easily in UART frame (max 250)
```

## 8. Log Manager

```cpp
class LogManager {
public:
    void init(uint8_t defaultLevel = LOG_INFO);

    // Set minimum level to forward to server
    void setLevel(uint8_t level);

    // Log methods
    void debug(const char* tag, const char* fmt, ...);
    void info(const char* tag, const char* fmt, ...);
    void warn(const char* tag, const char* fmt, ...);
    void error(const char* tag, const char* fmt, ...);

    // Forward pending logs via WS (batched)
    void flush(WebSocketClient& ws);

private:
    uint8_t minLevel_;

    // Ring buffer cho logs chưa gửi
    struct LogEntry {
        uint8_t level;
        char tag[16];
        char message[128];
        uint32_t timestamp;
    };

    RingBuffer<LogEntry, 64> buffer_;  // 64 entries max

    // Batch send timer (mỗi 5s hoặc khi buffer > 50%)
    bool shouldFlush() const;
};

// Macro wrappers
#define LLOG_D(tag, fmt, ...) LogManager::instance().debug(tag, fmt, ##__VA_ARGS__)
#define LLOG_I(tag, fmt, ...) LogManager::instance().info(tag, fmt, ##__VA_ARGS__)
#define LLOG_W(tag, fmt, ...) LogManager::instance().warn(tag, fmt, ##__VA_ARGS__)
#define LLOG_E(tag, fmt, ...) LogManager::instance().error(tag, fmt, ##__VA_ARGS__)
```

## 9. Inter-MCU Protocol Updates

### Phân chia vai trò SPI vs UART (hiện tại)

```
SPI (256-byte full-duplex, high bandwidth):
  S3→C5: AUDIO_UPLINK (Opus frames from mic)
  C5→S3: AUDIO_DOWNLINK (raw byte stream, Opus từ server)
  C5→S3: EMPTY (+ flow control: WS tx_buffer free KB)
  S3→C5: HEARTBEAT (khi không có audio → poll C5)

UART (variable-length, low bandwidth):
  S3→C5: CONTROL_CMD (START, END, SET_VOLUME, REBOOT, config...)
  C5→S3: STATUS_UPDATE (interaction, connectivity, system, emotion)
```

**Giữ nguyên nguyên tắc này.** SPI chỉ chạy audio, UART chạy control/status.
Các message mới (SYNC_DATA, OTA_STATUS, DEVICE_CMD) đều đi qua UART.

### Thêm UART message types mới

```cpp
// UartProtocol.hpp — mở rộng enum MsgType
enum class MsgType : uint8_t {
    // Existing
    STATUS_UPDATE  = 0x01,  // C5 → S3: state snapshot
    CONTROL_CMD    = 0x02,  // S3 → C5: command

    // New
    SYNC_DATA      = 0x03,  // C5 → S3: weather, time, calendar (from server)
    OTA_STATUS     = 0x04,  // C5 → S3: OTA state + progress
    LOG_ENTRY      = 0x05,  // S3 → C5: log from S3 side → forward to server
    DEVICE_CMD     = 0x06,  // C5 → S3: command from server/app (emotion, scene, etc.)
};
```

### Thêm UART ControlCmd mới

```cpp
// UartProtocol.hpp — mở rộng enum ControlCmd
enum class ControlCmd : uint8_t {
    // Existing
    START          = 0x01,
    END            = 0x02,
    SET_VOLUME     = 0x03,
    REBOOT         = 0x04,
    SET_BRIGHTNESS = 0x05,
    WIFI_CONFIG    = 0x10,
    WS_CONFIG      = 0x12,

    // Removed
    // MQTT_CONFIG = 0x11,   ← XOÁ (không còn MQTT)

    // New — relayed from WS commands (see WsProtocol.hpp MsgType mapping)
    // WS type "set_emotion" → DEVICE_CMD(SET_EMOTION) qua UART
    SET_EMOTION    = 0x20,  // [emotion_id:1] [variant_id:1]
    SET_SCENE      = 0x21,  // [scene_id:1] [variant_id:1]
    TTS_PLAY       = 0x22,  // [text: null-terminated]
    AUDIO_STOP     = 0x23,  // []
    CONFIG_UPDATE  = 0x24,  // [key:1] [value: null-terminated]
    INTERACTION_MSG= 0x25,  // [text: null-terminated] (from app/web user → robot TTS)
};
```

### SYNC_DATA payload format (UART, C5 → S3)

```
Fits trong UART frame (max 250 bytes):
┌────────────────────────────────────────────────────────────┐
│ temp:1  │ feels:1 │ humidity:1 │ condition:1(enum) │ aqi:1 │
│ unix_time:4 (LE)  │ utc_offset:1                          │
│ lunar_day:1 │ lunar_month:1                                │
│ city: null-terminated string                               │
└────────────────────────────────────────────────────────────┘
Total: ~40 bytes típ. — dư sức UART frame
```

### SPI protocol — KHÔNG thay đổi

SPI giữ nguyên chỉ 3 types: `AUDIO_UPLINK`, `AUDIO_DOWNLINK`, `HEARTBEAT/EMPTY`.
Không thêm message types vào SPI.

## 10. Opus Buffer Overflow — Frame Alignment Fix

### Vấn đề hiện tại

```
WS binary → C5 dl_audio_sb_ (48KB) → SPI raw bytes → S3 sb_spk_encoded (64KB) → Decode
                                         │
                                    KHÔNG frame-aligned
                                    (cắt giữa [2B len][opus])
                                         │
                                    Khi S3 buffer full, drop SPI payload
                                    → phá vỡ alignment vĩnh viễn
                                    → cascade decode errors
                                    → resync tốn thời gian + artifacts
```

**Root cause:** C5 `prepareTxFrame()` đọc raw bytes từ stream buffer, không quan tâm
opus frame boundaries. Một SPI frame 250 bytes có thể chứa cuối frame A + đầu frame B.
Khi S3 drop 1 SPI payload → byte stream mất alignment → header bị lệch → garbage decode.

### Giải pháp: Frame-aligned SPI transfer

Thay vì streaming raw bytes qua SPI, C5 gửi **complete opus frames** trong mỗi SPI payload.

```
Hiện tại (raw byte streaming):
  SPI payload: [... cuối frame A ...][2B len B][opus B...][2B len C][op...
               ← drop payload này → alignment bị phá

Sau fix (frame-aligned):
  SPI payload: [count:1][2B len A][opus A][2B len B][opus B][padding...]
               ← drop payload này → chỉ mất frame A, B (Opus PLC handles)
               → payload tiếp theo bắt đầu bằng count + header mới → alignment OK
```

### Implementation (C5 SpiBridge)

```cpp
// C5: prepareTxFrame — frame-aligned version
void SpiBridge::prepareTxFrame(uint8_t* tx_buf)
{
    if (!dl_audio_sb_ || xStreamBufferBytesAvailable(dl_audio_sb_) == 0) {
        // Empty frame
        if (ul_space_query_) {
            size_t free_bytes = ul_space_query_();
            uint8_t free_kb = (uint8_t)std::min(free_bytes / 1024, (size_t)255);
            spi_proto::buildFrame(tx_buf, (uint8_t)MsgFromC5::EMPTY, &free_kb, 1, tx_seq_++);
        } else {
            spi_proto::buildEmptyFrame(tx_buf, tx_seq_++);
        }
        return;
    }

    // Pack complete opus frames into SPI payload
    // Format: [frame_count:1] [2B len + opus data]... [padding]
    uint8_t payload[spi_proto::MAX_PAYLOAD];
    size_t pos = 1;  // byte 0 = frame count
    uint8_t frame_count = 0;

    while (pos + 2 < spi_proto::MAX_PAYLOAD) {
        // Peek 2-byte header without consuming
        size_t avail = xStreamBufferBytesAvailable(dl_audio_sb_);
        if (avail < 2) break;

        uint8_t header[2];
        // Use receive to read header
        size_t got = xStreamBufferReceive(dl_audio_sb_, header, 2, 0);
        if (got < 2) break;

        uint16_t frame_len = header[0] | ((uint16_t)header[1] << 8);
        if (frame_len == 0 || frame_len > 512) {
            // Corrupted header in stream — skip and try to resync
            ESP_LOGW(TAG, "Bad opus header in stream: %u, skipping", frame_len);
            break;
        }

        // Check if complete frame fits in remaining SPI payload
        size_t needed = 2 + frame_len;  // header + data
        if (pos + needed > spi_proto::MAX_PAYLOAD) {
            // Frame doesn't fit — put header back (push to secondary buffer)
            // or simply break and let it be first in next SPI frame.
            // Since we already consumed the header, we need to handle this.
            // Solution: use a small "pending header" buffer
            pending_header_[0] = header[0];
            pending_header_[1] = header[1];
            has_pending_header_ = true;
            break;
        }

        // Read opus data
        avail = xStreamBufferBytesAvailable(dl_audio_sb_);
        if (avail < frame_len) {
            // Incomplete frame in buffer — wait for more data
            pending_header_[0] = header[0];
            pending_header_[1] = header[1];
            has_pending_header_ = true;
            break;
        }

        // Write complete [2B len][opus] to payload
        payload[pos++] = header[0];
        payload[pos++] = header[1];
        got = xStreamBufferReceive(dl_audio_sb_, &payload[pos], frame_len, 0);
        pos += got;
        frame_count++;
    }

    if (frame_count > 0) {
        payload[0] = frame_count;
        spi_proto::buildFrame(tx_buf, (uint8_t)MsgFromC5::AUDIO_DOWNLINK,
                              payload, (uint16_t)pos, tx_seq_++);
    } else {
        spi_proto::buildEmptyFrame(tx_buf, tx_seq_++);
    }
}
```

### S3 Side Changes

```cpp
// S3: SpiBridge audio downlink callback — now receives frame-aligned data
// payload[0] = frame_count
// payload[1..] = sequence of [2B len][opus data]
// Each SPI payload is self-contained → dropping = losing complete frames only

spi_bridge->onAudioDownlink([spk_sb](const uint8_t* data, size_t len) {
    if (!data || len < 3) return;  // minimum: count(1) + header(2)

    // Skip frame_count byte, write remaining [2B len][opus]... to stream buffer
    // ALL frames in this payload are complete → write is safe
    const uint8_t* frames = data + 1;
    size_t frames_len = len - 1;

    size_t space = xStreamBufferSpacesAvailable(spk_sb);
    if (space >= frames_len) {
        xStreamBufferSend(spk_sb, frames, frames_len, 0);
    } else {
        // Buffer full: drop ENTIRE payload (all complete frames)
        // Alignment preserved — next payload starts fresh
        // Opus PLC handles the gap gracefully
    }
});
```

### Kết quả

| | Trước | Sau |
|--|-------|-----|
| Drop granularity | Raw bytes (phá alignment) | Complete opus frames |
| Sau drop | Cascade errors + resync 500ms | Opus PLC (tự nhiên, ~20ms gap) |
| Resync cần? | Có (byte scan + trial decode) | Không (alignment luôn đúng) |
| Audible artifact | Tiếng click/noise + 500ms silence | Nhẹ hoặc không nghe thấy |
| audioRecvLoop resync code | Vẫn giữ làm safety net | Hiếm khi trigger |

## 11. BLE Provisioning Updates

Thêm fields cho hệ thống mới — **phải match PLAN_APP.md §3.1**:

### 11.1 BLE Access Levels

```
Level 0 — No auth (mặc định khi BLE connect):
  Read:  DEVICE_INFO
  Write: (không có)

Level 1 — User auth (write pairing PIN vào AUTH_UNLOCK):
  Read:  DEVICE_INFO, SSID, WS_URL, USER_ID
  Write: SSID, PASSWORD, WS_URL, DEV_TOKEN, USER_ID, ADMIN_SECRET
  Cmd:   restart (0x01)

Level 2 — Admin auth (write admin token vào ADMIN_AUTH):
  Read:  tất cả Level 1 + DIAG_INFO, LOG_LEVEL, NVS keys
  Write: tất cả Level 1 + LOG_LEVEL, WS_URL (đổi domain)
  Cmd:   tất cả Level 1 + factory_reset, full_wipe, rollback_fw,
         set_debug, clear_users, enter_dfu
```

### 11.2 Characteristics

```cpp
// BluetoothService — updated characteristics
// Service UUID: 0xFF01
enum BleCharacteristic {
    // === Provisioning (Level 1+) ===
    CHAR_SSID         = 0x0001,  // WiFi SSID (R/W, max 32 bytes)
    CHAR_PASSWORD     = 0x0002,  // WiFi password (W-only, max 64 bytes)
    CHAR_WS_URL       = 0x0003,  // Server URL (R/W, max 128 bytes)
    // 0x0004 — removed (device_token replaced by MAC identity)
    CHAR_USER_ID      = 0x0005,  // Owner user ID (R/W, max 36 bytes, UUID)

    // === Info (Level 0+) ===
    CHAR_DEVICE_INFO  = 0x0006,  // Device info (R-only, JSON)
    CHAR_DIAG_INFO    = 0x0007,  // Extended diagnostics (R-only, Level 2, JSON)

    // === Security ===
    CHAR_AUTH_UNLOCK   = 0x0010, // Pairing PIN → Level 1 (6 chars hiển thị trên màn hình robot)
    CHAR_ADMIN_AUTH    = 0x0012, // Admin token → Level 2 (HMAC-SHA256, server cấp)

    // === Control ===
    CHAR_COMMAND       = 0x0011, // Write command byte (xem bảng dưới)
    CHAR_LOG_LEVEL     = 0x0013, // R/W, 1 byte: 10=DEBUG, 20=INFO, 30=WARN, 40=ERROR (Level 2)

    // === Admin Secret (written during pairing) ===
    CHAR_ADMIN_SECRET  = 0x0014, // W-only, Level 1, 32 bytes — key để verify admin BLE token
};
```

### 11.3 COMMAND values

```cpp
// Commands — ghi 1 byte vào CHAR_COMMAND
enum class BleCommand : uint8_t {
    // Level 1 — User
    RESTART           = 0x01,  // Khởi động lại device

    // Level 2 — Admin only
    FACTORY_RESET     = 0x10,  // Xóa WiFi + token, giữ firmware
    FULL_WIPE         = 0x11,  // Xóa toàn bộ NVS (WiFi, token, user, config)
    ROLLBACK_FW       = 0x12,  // Roll back firmware về partition trước
    ENABLE_DEBUG      = 0x13,  // Bật debug log (tạm, reset khi reboot)
    DISABLE_DEBUG     = 0x14,  // Tắt debug log
    CLEAR_USERS       = 0x15,  // Xóa tất cả user IDs liên kết
    ENTER_DFU         = 0x16,  // Vào chế độ DFU (firmware download qua UART/USB)
};

// Device response: notify trên CHAR_COMMAND sau khi thực hiện
// 0x00 = OK, 0x01 = FAIL, 0x02 = UNAUTHORIZED (level không đủ)
```

### 11.4 Admin Auth Flow

```
App (admin)                              Robot (ESP32-C5)
    │                                        │
    │── BLE Connect ─────────────────────────▶│
    │── Read CHAR_DEVICE_INFO ───────────────▶│ Level 0 (public)
    │◀── { mac, model, fw_version, name } ───│
    │                                        │
    │── Write CHAR_AUTH_UNLOCK (PIN "182453")▶│ PIN hiển thị trên màn hình robot
    │◀── Notify: 0x00 (OK) ──────────────────│ → Level 1
    │                                        │
    │── App calls server: ───────────────────│
    │   POST /api/v1/devices/:mac/ble-token  │
    │   → server returns admin_token         │
    │   (HMAC-SHA256(mac + ts,               │
    │    admin_secret))                      │
    │                                        │
    │── Write CHAR_ADMIN_AUTH ───────────────▶│
    │   { token: "hex64", ts: epoch }        │ Device verify:
    │                                        │  HMAC(mac + ts, admin_secret)
    │                                        │  Check ts within 5 min
    │◀── Notify: 0x00 (OK) ──────────────────│ → Level 2
    │                                        │
    │── Admin commands now available ────────│
    │── Write COMMAND(FACTORY_RESET 0x10) ──▶│ Accepted (Level 2)
    │── Read CHAR_DIAG_INFO ────────────────▶│ Accepted (Level 2)
    │◀── { heap, uptime, reset_reason,       │
    │      nvs_usage, partition_info, ... }  │
```

**admin_secret provisioning:**
- Server derives: `admin_secret = HMAC-SHA256(mac, SERVER_SECRET_KEY)`
- Server trả `admin_secret` trong response của `POST /api/v1/devices` (register)
- App ghi `admin_secret` vào device qua BLE (`CHAR_ADMIN_SECRET` 0x0014)
- `admin_secret` ổn định vì MAC không đổi — chỉ thay khi admin rotate SERVER_SECRET_KEY

**Admin token verification trên device:**
- `admin_secret` lưu trong NVS, provisioned qua BLE trong lúc pairing
- Verify: `HMAC-SHA256(mac || timestamp, admin_secret) == received_token`
- MAC = BLE MAC (ESP_MAC_BT), vĩnh viễn, survive NVS reset
- Timestamp phải trong ±5 phút (device dùng NTP time hoặc millis delta)
- Token dùng 1 lần (replay protection)

### 11.5 DEVICE_INFO vs DIAG_INFO

```cpp
// CHAR_DEVICE_INFO (0x0006) — Level 0, public, ~150 bytes JSON
// { "mac": "AA:BB:CC:DD:EE:FF", "model": "luni_v2_s3c5",
//   "fw_version": "2.1.0", "name": "Luni" }

// CHAR_DIAG_INFO (0x0007) — Level 2, admin only, ~300 bytes JSON
// { "free_heap": 45032, "min_heap": 28000,
//   "uptime_s": 86400, "reset_reason": "power_on",
//   "nvs_used": "12/64 entries",
//   "wifi_rssi": -42, "wifi_ssid": "MyNetwork",
//   "ws_state": "ONLINE", "fw_partition": "ota_0",
//   "ota_rollback_available": true,
//   "spi_errors": 0, "uart_errors": 2,
//   "battery_mv": 3850, "charging": false,
//   "s3_fw_version": "2.1.0", "s3_state": "RUNNING" }
```

### 11.6 NVS Keys & Reset Behavior

| NVS Key | factory_reset (0x10) | full_wipe (0x11) |
|---|---|---|
| `wifi_ssid` | ERASE | ERASE |
| `wifi_password` | ERASE | ERASE |
| ~~`device_token`~~ | — (removed: MAC is device identity) | — |
| `admin_secret` | ERASE | ERASE |
| `ws_url` | KEEP | ERASE |
| `user_id` | KEEP | ERASE |
| `device_name` | KEEP | ERASE |
| `log_level` | KEEP (reset→INFO) | ERASE |
| FW partitions | KEEP | KEEP |

**Sau factory_reset:** Device giữ server URL + name → khi re-pair chỉ cần WiFi + token mới.
**Sau full_wipe:** Device hoàn toàn clean → cần setup toàn bộ như lần đầu.

### 11.7 Reboot-causing Commands — Sequence

Commands gây reboot: `RESTART`, `FACTORY_RESET`, `FULL_WIPE`, `ROLLBACK_FW`

```
Device nhận command:
  1. Validate access level (Level 1 cho RESTART, Level 2 cho các lệnh khác)
  2. Fail → notify 0x02 (UNAUTHORIZED) → dừng
  3. OK → notify 0x00 (OK) trên CHAR_COMMAND
  4. Delay 500ms (đảm bảo BLE notification delivered)
  5. Thực hiện action (erase NVS nếu cần)
  6. esp_restart()

App handling:
  1. Ghi command → wait notify OK/FAIL (timeout 3s)
  2. OK → disconnect BLE sau 1s
  3. Expect device reboot (~3-5s)
  4. RESTART: poll API /devices/:id/status → verify online
  5. FACTORY_RESET / FULL_WIPE: expect device ở BLE_PROVISIONING → re-pair
  6. ROLLBACK_FW: expect reboot → online with older fw_version
```

### 11.8 CLEAR_USERS Scope

`CLEAR_USERS (0x15)`: Xóa `user_id` trong NVS local.

- Server-side `devices.owner_id` và `device_shares` **KHÔNG** bị ảnh hưởng.
- Device vẫn kết nối server bình thường (dùng MAC identity, không dùng `user_id`).
- Để xóa quyền truy cập server-side → dùng web admin hoặc API `DELETE /devices/:id/shares`.
- Use case: reset device local identity trước khi chuyển cho user khác (kết hợp factory_reset).

## 12. ESP32-S3 Changes

### DisplayManager — sync_data integration

```cpp
// DisplayManager receives sync_data from C5 via UART
void DisplayManager::handleSyncData(const SyncDataPacket& data) {
    // Update SceneManager's live data
    sceneManager_.updateLiveData({
        .unixTime = data.unixTime,
        .temperature = data.temperature,
        .humidity = data.humidity,
        .condition = data.condition,
        .city = data.city,
        .lunarDay = data.lunarDay,
        .lunarMonth = data.lunarMonth,
    });

    // If weather scene active, trigger re-render
    if (sceneManager_.currentCategory() == "weather") {
        sceneManager_.refresh();
    }
}
```

### SceneManager — live data consumption

```cpp
// SceneManager already has LiveData concept (from SCENE_DATA_SPEC.md)
// Refactor to use server-pushed data instead of local NTP/API calls

struct LiveData {
    // Time (from server sync)
    int64_t  unixTime;
    int8_t   utcOffset;

    // Weather (from server sync)
    int8_t   temperature;
    uint8_t  humidity;
    uint8_t  weatherCondition;  // enum: clear, cloudy, rain, snow, ...
    uint8_t  aqi;

    // Calendar (from server sync)
    uint8_t  lunarDay;
    uint8_t  lunarMonth;

    // Local (from S3 sensors)
    uint8_t  batteryPercent;
    bool     isCharging;
    int8_t   wifiRssi;

    // Computed
    uint8_t  hour() const;
    uint8_t  minute() const;
    bool     isDaytime() const;
    const char* dayOfWeek() const;

    bool isValid() const { return unixTime > 0; }
    uint32_t age() const;   // millis since last update
};
```

## 13. Migration Checklist

### Phase 1: Foundation
- [ ] Tạo `StateTypes.hpp` mới với đầy đủ state + transition rules
- [ ] Tạo `WsProtocol.hpp` — message types, builders, parser
- [ ] Refactor `WebSocketClient` — thêm binary frame, auth, text/binary callbacks
- [ ] Tạo `HttpClient` — lightweight HTTP cho OTA
- [ ] Tạo `LogManager` — structured logging
- [ ] Update `StateManager` — support transition validation

### Phase 2: Network
- [ ] Viết lại `NetworkManager` — connection state machine
- [ ] Tạo `WsMessageHandler` — dispatch incoming commands
- [ ] Tạo `OtaManager` — HTTP-based OTA flow
- [ ] Tạo `DataSyncManager` — receive + cache sync_data
- [ ] Update `BluetoothService` — new BLE characteristics

### Phase 3: Integration
- [ ] Update UART protocol — thêm SYNC_DATA, OTA_STATUS, LOG_ENTRY, DEVICE_CMD message types
- [ ] Update C5 `UartBridge` — gửi SYNC_DATA, OTA_STATUS, DEVICE_CMD qua UART
- [ ] Update S3 `UartBridge` — nhận + dispatch new UART message types
- [ ] Fix Opus buffer overflow — frame-aligned SPI transfer (C5 `prepareTxFrame` + S3 callback)
- [ ] Update S3 `DisplayManager` — consume sync_data từ UART
- [ ] Update S3 `SceneManager` — LiveData from server (qua UART relay)
- [ ] Update S3 `StateTypes.hpp` — sync with C5

### Phase 4: Cleanup
- [ ] Xoá `MqttClient.cpp/hpp`
- [ ] Xoá `MQTTConfig.hpp`
- [ ] Xoá MQTT-related code trong `NetworkManager` cũ
- [ ] Xoá MQTT-related NVS keys (`mqtt_url`)
- [ ] Update BLE: remove MQTT URL characteristic
- [ ] Test full flow: boot → WiFi → WS → auth → sync_data → audio → OTA

## 14. Testing Strategy

### Simulator updates (`sim/`)

```python
# sim/server/server.py — update to match new protocol
# - Remove MQTT server
# - WebSocket-only with text/binary frame support
# - Implement sync_data push
# - Implement OTA check endpoint
# - Implement audio echo (for testing)

# sim/device/device_sim.py — update to match new protocol
# - Remove MQTT client
# - WS-only with JSON messages
# - Binary audio frames
# - Connection state machine simulation
```

### Hardware test checklist
1. Boot → WiFi connect → WS connect → Auth → sync_data received
2. Button press → audio stream → server response → speaker playback
3. App send command → WS → C5 → UART → S3 → execute
4. OTA: server push notification → HTTP download → verify → flash → reboot
5. WiFi lost → reconnect → WS reconnect → resume
6. BLE provisioning → new credentials → WiFi + WS connect
7. Multiple state transitions: no crashes, no deadlocks
8. Log forwarding: device logs appear on web admin
9. Weather/time scene: correct data from server sync
