# Scene System & Real-Time Data Specification

How scenes are registered, how they receive live data (time, weather,
network status), and how the boot/network state machines work.

---

## 1. Scene Category Registry

### 1.1 All 21 Scene Categories

| Category | Variants | Default Tone | Data Source | Real-time? |
|----------|----------|-------------|-------------|-----------|
| boot | 4 | red | Internal FSM | No |
| network | 6 | cyan | ConnectionState | Yes |
| weather | 5 | cyan | OpenWeather API | Periodic (10 min) |
| clock | 3 | cyan | NTP | Yes (1 Hz) |
| music | 3 | rose | App/server event | Event |
| timer | 3 | orange | User-triggered | Yes (1 Hz) |
| cooking | 2 | orange | User-triggered | Yes (1 Hz) |
| walking | 2 | green | Sensor/API | Periodic |
| celebrate | 3 | warm | Event-triggered | No |
| night | 2 | purple | Clock-derived | Periodic |
| fitness | 3 | red | Sensor/API | Periodic |
| call | 3 | green | Server event | Event |
| message | 3 | cyan | Server event | Event |
| camera | 2 | white | Event-triggered | No |
| navigation | 3 | cyan | API | Periodic |
| gift | 2 | rose | Event-triggered | No |
| coffee | 2 | warm | Event-triggered | No |
| plant | 2 | green | Timer/sensor | Periodic |
| morning | 2 | warm | Clock-derived | Event |
| gaming | 2 | purple | Event-triggered | No |
| calendar | 2 | cyan | Calendar API | Periodic |

### 1.2 Complete Variant List with Tones

See `ui_design/REQUIREMENTS.md` §8b for the full inventory. Key overrides
from `emotion-color.jsx` SCENE_VARIANT_TONE:

```
boot:       all red
network:    wifi-scan/connect=cyan, retry=orange, offline/server-error=red, ble-pair=purple
weather:    sunny=warm, rainy=blue, cloudy=cyan, snow=white, storm=purple
clock:      digital/analog=cyan, alarm=red
music:      notes=rose, bars=cyan, wave=purple
timer:      countdown=orange, progress=cyan, hourglass=warm
celebrate:  trophy=warm, confetti=multi, firework=multi
fitness:    hr=red, steps=green, dumbbell=warm
call:       incoming=green, active=cyan, missed=red
navigation: compass=cyan, pin=red, arrow=green
```

---

## 2. C++ Scene/Variant Types

### 2.1 Variant Registry

```cpp
// esp32-s3/src/ui/VariantRegistry.hpp

using RenderFn = void(*)(GfxEngine& gfx, float t, const ColorContext& colors);

struct VariantDef {
    const char* id;           // "boot-poweron", "weather-sunny"
    const char* label;        // Human-readable label
    uint16_t duration_ms;     // Animation cycle duration
    ToneId tone;              // TONE_NONE = use category default
    RenderFn render;          // C++ render function pointer
};

enum class ContentKind : uint8_t { EMOTION, SCENE };

struct CategoryDef {
    const char* key;          // "boot", "weather", "normal"
    const char* label;        // "Boot", "Weather", "Normal"
    ContentKind kind;
    ToneId tone;              // Default tone for the category
    const VariantDef* variants;
    uint8_t variant_count;
};
```

### 2.2 Tone Resolution

```cpp
ToneId resolveTone(const CategoryDef* cat, const VariantDef* variant) {
    if (variant->tone != TONE_NONE) return variant->tone;
    return cat->tone;
}
```

---

## 3. Scene Lifecycle

### 3.1 State Diagram

```
     ┌────────┐
     │  IDLE  │ ← emotion loop running (normal idle, or active emotion)
     └───┬────┘
         │ showScene("weather", "sunny")
         ▼
     ┌────────┐
     │ ENTER  │ → store scene data, resolve colors, reset t=0
     └───┬────┘
         │
         ▼
     ┌────────┐
     │ RENDER │ ← called every frame: render(gfx, t, colors)
     │  LOOP  │   t cycles [0,1) over duration_ms
     └───┬────┘
         │ updateSceneData(newData) → hot-update without reset
         │ exitScene() or timeout
         ▼
     ┌────────┐
     │  EXIT  │ → resume emotion loop
     └────────┘
```

### 3.2 SceneManager API

```cpp
// esp32-s3/src/ui/SceneManager.hpp

class SceneManager {
public:
    // Show a scene — blocks emotion system until exit
    void showScene(const char* categoryKey, const char* variantId = nullptr);

    // Update live data while scene is playing (no t reset)
    void updateSceneData(const SceneData& data);

    // Exit current scene, resume emotions
    void exitScene();

    // Is a scene currently blocking?
    bool isSceneActive() const;

    // Render current frame (called by DisplayManager)
    void render(GfxEngine& gfx, float dt_ms);

    // Access current scene data (called by render functions)
    const SceneData& getSceneData() const;

    // --- Idle Emotion Management ---

    // Pick a random variant from a category (with recency filter)
    const VariantDef* pickVariant(const char* categoryKey);

    // Idle tick — picks from "normal" every 3-6s when no scene active
    void tickIdle(float dt_ms);

private:
    const VariantDef* current_variant_ = nullptr;
    const CategoryDef* current_category_ = nullptr;
    float elapsed_ms_ = 0;
    bool scene_active_ = false;

    // Recency buffer prevents back-to-back repeats
    static constexpr int RECENT_SIZE = 6;
    const char* recent_ids_[RECENT_SIZE] = {};
    int recent_head_ = 0;

    // Idle timer
    float idle_timer_ms_ = 0;
    float idle_interval_ms_ = 4000; // randomized 3-6s

    // Live data for data-driven scenes
    SceneData scene_data_;
};
```

### 3.3 Scene Selection Rules

From `ui_design/REQUIREMENTS.md` §10:

- **Emotions** enter the random idle pool. Firmware picks from `normal`
  every 3-6 seconds when idle.
- **Scenes** are picked explicitly by the host when there is data to
  display. They never enter the random pool.
- Use `showScene('weather', 'sunny')` → display → `exitScene()` → return
  to emotion loop.
- While a scene is active, the emotion system is suspended.

---

## 4. SceneData — Live Data Structure

```cpp
// esp32-s3/src/ui/SceneManager.hpp

struct SceneData {
    // --- Time (from server sync via C5) ---
    uint8_t  hours   = 0;       // 0-23
    uint8_t  minutes = 0;       // 0-59
    uint8_t  seconds = 0;       // 0-59
    uint8_t  day_of_week = 0;   // 0=Sun, 6=Sat
    uint8_t  day     = 1;       // 1-31
    uint8_t  month   = 1;       // 1-12
    uint16_t year    = 2026;
    bool     time_valid = false;
    int64_t  unix_time = 0;     // Unix timestamp from server
    int8_t   utc_offset = 0;    // UTC offset in hours

    // --- Weather (from server sync via C5) ---
    int8_t   temperature = 0;          // Celsius
    uint8_t  humidity = 0;             // 0-100%
    uint8_t  weather_condition = 0;    // Enum: 0=unknown, 1=clear, ...
    uint8_t  aqi = 0;                  // Air quality index
    bool     weather_valid = false;

    // --- Calendar (from server sync via C5) ---
    uint8_t  lunar_day = 0;
    uint8_t  lunar_month = 0;

    // --- Location (from server sync via C5) ---
    char     city[32] = {};

    // --- Boot ---
    uint8_t  boot_progress_pct = 0;    // 0-100
    uint8_t  boot_check_results[5] = {}; // 0=pending, 1=ok, 2=fail
    // Indices: 0=DISPLAY, 1=AUDIO, 2=MIC, 3=NETWORK, 4=AI_CORE

    // --- Network ---
    char     ssid[33] = {};            // Connected/target SSID
    uint8_t  retry_attempt = 0;        // Current retry number
    uint8_t  retry_max = 3;
    uint16_t error_code = 0;           // HTTP/server error code

    // --- Local sensors (S3) ---
    uint8_t  battery_percent = 0;
    bool     is_charging = false;
    int8_t   wifi_rssi = 0;
};
```

> **Note**: SceneData is defined in `SceneManager.hpp`. Weather data arrives
> via UART SYNC_DATA (0x03) as a binary-packed payload from C5's
> `DataSyncManager`, not as individual TIME_SYNC/WEATHER_UPDATE messages.
> The C5 parses the server's JSON `sync_data` WS message and relays a compact
> binary: `[temp][humidity][condition][aqi][unix_time:4LE][utc_offset][lunar_day][lunar_month][city:null-term]`.

---

## 5. Data Flow Architecture

```
┌──────────────────────────────────────────┐
│               ESP32-C5                    │
│  (WiFi + BLE + WebSocket)                │
│                                           │
│  ┌─────────────┐  ┌──────────────┐       │
│  │ WebSocket   │  │ DataSync     │       │
│  │ Client(WSS) │  │ Manager      │       │
│  └──────┬──────┘  └──────┬───────┘       │
│         │                │                │
│         ▼                ▼                │
│  ┌──────────────────────────────┐        │
│  │  UartBridge (TX to S3)      │        │
│  │  STATUS_UPDATE / SYNC_DATA  │        │
│  │  OTA_STATUS / DEVICE_CMD    │        │
│  └──────────────┬───────────────┘        │
└─────────────────┼────────────────────────┘
                  │ UART1 (115200 baud)
                  ▼
┌──────────────────────────────────────────┐
│               ESP32-S3                    │
│                                           │
│  ┌──────────────────────────────┐        │
│  │  UartBridge (RX from C5)    │        │
│  └──────────────┬───────────────┘        │
│                 │                         │
│                 ▼                         │
│  ┌──────────────────────────────┐        │
│  │  StateManager                │        │
│  │  + ConnectionState (from C5) │        │
│  │  + OtaState (from C5)        │        │
│  │  + SyncData (weather/time)   │        │
│  └──────────────┬───────────────┘        │
│                 │ state change events     │
│                 ▼                         │
│  ┌──────────────────────────────┐        │
│  │  SceneManager                │        │
│  │  → reads SceneData           │        │
│  │  → calls render functions    │        │
│  └──────────────┬───────────────┘        │
│                 │                         │
│                 ▼                         │
│  ┌──────────────────────────────┐        │
│  │  GfxEngine → DisplayDriver   │        │
│  └──────────────────────────────┘        │
└──────────────────────────────────────────┘
```

---

## 6. UART Protocol Extension

The existing protocol (`UartProtocol.hpp`) uses a simple frame format:
```
[0x55 start][type:1][len:1][payload:0-250][crc8:1]
```

### 6.1 Message Types (Implemented)

```cpp
enum class MsgType : uint8_t {
    STATUS_UPDATE  = 0x01,  // C5 → S3: interaction + connection + system + emotion
    CONTROL_CMD    = 0x02,  // S3 → C5: START, END, SET_VOLUME, REBOOT, etc.
    SYNC_DATA      = 0x03,  // C5 → S3: weather, time, lunar, city (binary packed)
    OTA_STATUS     = 0x04,  // C5 → S3: OTA state + progress percent
    LOG_ENTRY      = 0x05,  // S3 → C5: log from S3, forwarded to server via WS
    DEVICE_CMD     = 0x06,  // C5 → S3: server commands (emotion, scene, TTS, etc.)
};
```

### 6.2 SYNC_DATA Payload (~40 bytes, from DataSyncManager)

C5 receives a JSON `sync_data` message via WebSocket, parses it, and
relays a compact binary to S3:

```
Byte 0:     temperature (int8_t, Celsius)
Byte 1:     humidity (uint8_t, 0-100)
Byte 2:     weather_condition (enum)
Byte 3:     aqi (uint8_t)
Byte 4-7:   unix_time (uint32_t LE)
Byte 8:     utc_offset (int8_t, hours)
Byte 9:     lunar_day (uint8_t)
Byte 10:    lunar_month (uint8_t)
Byte 11+:   city (null-terminated string, max ~30 chars)
```

S3's `DisplayManager::handleSyncData()` unpacks this into `SceneData`.

### 6.3 OTA_STATUS Payload (2 bytes)

```
Byte 0: ota_state (OtaState enum: IDLE..FAILED)
Byte 1: progress_percent (0-100)
```

### 6.4 DEVICE_CMD Payload (variable)

```
Byte 0:   sub-command (ControlCmd enum)
Byte 1+:  command-specific data
```

Sub-commands: SET_EMOTION (0x20), SET_SCENE (0x21), TTS_PLAY (0x22),
AUDIO_STOP (0x23), CONFIG_UPDATE (0x24), INTERACTION_MSG (0x25).

### 6.5 StatusPayload (4 bytes)

```cpp
struct StatusPayload {
    uint8_t interaction;   // InteractionState enum
    uint8_t connection;    // ConnectionState enum
    uint8_t system_state;  // SystemState enum
    uint8_t emotion;       // EmotionState enum
};
```

---

## 7. StateManager Extension

### 7.1 State Types (Implemented)

S3 `StateTypes.hpp` now contains:

```cpp
// ConnectionState (mirrored from C5 via UART STATUS_UPDATE)
enum class ConnectionState : uint8_t {
    OFFLINE, WIFI_CONNECTING, WIFI_CONNECTED, WS_CONNECTING,
    WS_AUTHENTICATING, ONLINE, RECONNECTING, BLE_PROVISIONING,
};

// InteractionState (shared S3 + C5)
enum class InteractionState : uint8_t {
    IDLE, TRIGGERED, LISTENING, PROCESSING, SPEAKING, CANCELLING,
};

// OtaState (from C5 via UART OTA_STATUS)
enum class OtaState : uint8_t {
    IDLE, CHECKING, AVAILABLE, DOWNLOADING, VERIFYING,
    FLASHING, REBOOTING, FAILED,
};

// EmotionState (16 values, from C5 via WS)
enum class EmotionState : uint8_t {
    NEUTRAL, HAPPY, SAD, ANGRY, CONFUSED, EXCITED, CALM, THINKING,
    DISGUSTED, NERVOUS, EMBARRASSED, CURIOUS, ANNOYED, COOL,
    SUSPICIOUS, DETERMINED,
};
```

Time/weather data flows through `SceneData` (populated by
`DisplayManager::handleSyncData()`) rather than separate state types.

### 7.2 Future EmotionState Expansion

The current 16-value EmotionState covers the core emotions. When UI
porting (Phase D) requires additional emotions (37 total), the enum will
be extended. The UART StatusPayload already uses `uint8_t` so it
supports up to 255 values without protocol changes.

---

## 8. Boot Sequence

### 8.1 Boot FSM

Source: `ui_design/REQUIREMENTS.md` §6.5.1 and `scenes-boot.jsx`.

```cpp
enum class BootPhase : uint8_t {
    POWERON,      // boot-poweron  (2400 ms)
    LOGO,         // boot-logo     (3000 ms)
    CHECKS,       // boot-checks   (4200 ms)
    CONNECTIVITY, // network scene sub-flow (variable)
    READY,        // boot-ready    (3000 ms)
    COMPLETE      // hand off to emotion idle loop
};
```

### 8.2 Boot Flow Sequence

```
Power on
  │
  ▼
POWERON (2.4s)
  │ Dot → horizontal bar → splits into two eyes
  ▼
LOGO (3.0s)
  │ Luni mark fade-in + sweep ring
  ▼
CHECKS (4.2s)
  │ Checklist: DISPLAY → AUDIO → MIC → NETWORK → AI CORE
  │ Each item shows spinner → "OK ✓" staggered 0.15s apart
  │ SceneData.boot_check_results[] updated per subsystem init
  ▼
CONNECTIVITY (variable)
  │ Enter network FSM (see §9)
  │ Blocks until ONLINE or timeout
  ▼
READY (3.0s)
  │ Progress bar fills → "READY" stamp
  ▼
COMPLETE
  │ Hand off to normal idle emotion loop
```

### 8.3 Integration with AppController

The boot sequence should be asynchronous. While boot scenes play,
subsystems initialize in background tasks:

```cpp
void AppController::boot() {
    scene_mgr_->showScene("boot", "boot-poweron");
    // While poweron plays (2.4s), init subsystems:

    // Display already initialized (we're rendering)
    scene_data_.boot_check_results[0] = 1; // DISPLAY: OK

    initAudio();
    scene_data_.boot_check_results[1] = 1; // AUDIO: OK

    initMic();
    scene_data_.boot_check_results[2] = 1; // MIC: OK

    // After poweron → logo → checks, enter connectivity
    // Network init happens during CONNECTIVITY phase
    scene_data_.boot_check_results[3] = ...; // NETWORK: depends on wifi

    // AI Core check (WebSocket to server)
    scene_data_.boot_check_results[4] = ...; // AI CORE: depends on WS

    // After all checks → READY → COMPLETE
}
```

---

## 9. Network State Machine

### 9.1 Network FSM

Source: `ui_design/REQUIREMENTS.md` §6.5.2 and `scenes-network.jsx`.

```
                poweron complete
                     │
                     ▼
           ┌─────────────────┐
           │ wifi-scan       │ ← no stored SSID
           │ "SEARCHING"     │
           └────────┬────────┘
                    │ SSID known           user holds button
                    ▼                           │
           ┌─────────────────┐           ┌──────▼──────┐
           │ wifi-connect    │           │ ble-pair    │
           │ "CONNECTING"    │           │ "PAIRING"   │
           │ + progress bar  │           └─────────────┘
           └────────┬────────┘
                    │
          ┌─────────┼──────────┐
          │ success │          │ fail (3 attempts)
          ▼         │          ▼
    ┌───────────┐   │   ┌──────────────┐
    │ (wifi up) │   │   │ wifi-retry   │
    └─────┬─────┘   │   │ "RETRY 1/3"  │
          │         │   └──────┬───────┘
          │         │          │ exhausted
          │         │          ▼
          │         │   ┌──────────────┐
          │         │   │ offline      │
          │         │   │ "NO INTERNET"│
          │         │   └──────────────┘
          │         │
          ▼         │
    ┌───────────────┴───┐
    │ ping cloud server │
    └────────┬──────────┘
             │
    ┌────────┼──────────┐
    │ ok     │          │ fail
    ▼        │          ▼
  ┌──────┐   │   ┌──────────────┐
  │ DONE │   │   │ server-error │
  │      │   │   │ "ERROR 503"  │
  └──────┘   │   │ (poll+retry) │
             │   └──────────────┘
```

### 9.2 Mapping ConnectionState to Scene Variants

```cpp
void DisplayManager::handleConnectivity(ConnectionState state) {
    switch (state) {
    case ConnectionState::OFFLINE:
        scene_mgr_->showScene("network", "network-offline");
        break;
    case ConnectionState::WIFI_CONNECTING:
        scene_mgr_->showScene("network", "network-wifi-connect");
        break;
    case ConnectionState::WIFI_CONNECTED:
    case ConnectionState::WS_CONNECTING:
    case ConnectionState::WS_AUTHENTICATING:
        // Keep wifi-connect scene while handshaking
        break;
    case ConnectionState::ONLINE:
        scene_mgr_->exitScene(); // Return to emotion idle loop
        break;
    case ConnectionState::RECONNECTING:
        scene_mgr_->showScene("network", "network-wifi-retry");
        break;
    case ConnectionState::BLE_PROVISIONING:
        scene_mgr_->showScene("network", "network-ble-pair");
        break;
    }
}
```

### 9.3 Network Scenes Block Emotions

While any `network-*` scene is playing, the emotion system is suspended.
User-facing emotions should not interrupt connectivity status displays.

---

## 10. Time & Weather Data Integration

### 10.1 Data Flow (Implemented)

Time, weather, lunar, and location data are delivered as a single
`sync_data` JSON message via WebSocket from the server:

```json
{
    "type": "sync_data",
    "payload": {
        "time": { "unix": 1716900000, "utc_offset": 7 },
        "weather": { "temp": 32, "humidity": 75, "condition": "clear", "aqi": 45 },
        "lunar": { "day": 15, "month": 4 },
        "location": { "city": "Ho Chi Minh" }
    }
}
```

### 10.2 ESP32-C5 Side (DataSyncManager)

`DataSyncManager` parses the JSON and builds a compact binary payload
(~40 bytes), then sends it via UART `SYNC_DATA` (0x03) to S3:

```cpp
void DataSyncManager::handleSyncData(cJSON* payload) {
    // Parse nested objects: time, weather, lunar, location
    // Build binary: [temp][humidity][condition][aqi][unix:4LE][utc_offset]
    //               [lunar_day][lunar_month][city:null-term]
    relayToS3(uart_bridge);
}
```

### 10.3 ESP32-S3 Side (DisplayManager)

`DisplayManager::handleSyncData()` unpacks the binary into `SceneData`
fields, making them available to scene render functions:

```cpp
void DisplayManager::handleSyncData(const state::SyncDataPacket& pkt) {
    auto& sd = SceneManager::instance().getSceneData();
    sd.temperature = pkt.temperature;
    sd.humidity = pkt.humidity;
    sd.weather_condition = pkt.condition;
    sd.aqi = pkt.aqi;
    sd.unix_time = pkt.unix_time;
    sd.utc_offset = pkt.utc_offset;
    sd.lunar_day = pkt.lunar_day;
    sd.lunar_month = pkt.lunar_month;
    memcpy(sd.city, pkt.city, sizeof(sd.city));
    sd.weather_valid = true;
    sd.time_valid = true;
}
```

### 10.4 Status Bar Time Display

The status bar shows current time (top-left, cyan, 11px monospace):

```cpp
void StatusBar::render(GfxEngine& gfx, const SceneData& data) {
    if (!data.time_valid) return;
    char buf[6]; // "HH:MM"
    snprintf(buf, sizeof(buf), "%02d:%02d", data.hours, data.minutes);
    gfx.drawText(buf, 8, 14, FONT_11, TONE_LUT[TONE_CYAN],
                 TextAlign::LEFT, 217); // 85% opacity
}
```

---

## 11. Weather Data & Scene Rendering

### 11.1 Weather Condition Mapping

The server provides weather condition as a string in `sync_data`.
`DataSyncManager::conditionToEnum()` maps it:

| Server String | Enum Value | Scene Variant | Tone |
|--------------|------------|---------------|------|
| "clear"      | 1          | weather-sunny | warm |
| "cloudy"     | 2          | weather-cloudy | cyan |
| "rain"       | 3          | weather-rainy | blue |
| "snow"       | 4          | weather-snow | white |
| "storm"      | 5          | weather-storm | purple |

### 11.2 Weather Scene Rendering

Weather scenes are data-driven — they read temperature and condition from
`SceneData`:

```cpp
void render_weather_sunny(GfxEngine& gfx, float t, const ColorContext& colors) {
    auto& data = SceneManager::instance().getSceneData();

    // Sun icon with rotating rays
    int16_t cx = SCX - 60, cy = CY - 10;
    gfx.pushTransform();
    gfx.translate(cx, cy);
    gfx.rotate(t * 180.0f, 0, 0);
    for (int i = 0; i < 8; i++) {
        float a = (float)i / 8.0f * math::TAU;
        gfx.drawLine(cosf(a)*36, sinf(a)*36, cosf(a)*52, sinf(a)*52,
                      colors.eye, 4);
    }
    gfx.popTransform();

    gfx.fillCircle(cx, cy, 28, colors.eye);
    gfx.fillCircle(cx, cy, 20, colors.bg);

    // Temperature from live data
    char temp[8];
    snprintf(temp, sizeof(temp), "%d\xC2\xB0", data.temperature);
    gfx.drawText(temp, SCX + 60, cy - 4, FONT_36, colors.eye, TextAlign::CENTER);
}
```

---

## 12. Scene Priority and Preemption

When multiple scene triggers arrive simultaneously, use this priority:

| Priority | Category | Reason |
|----------|----------|--------|
| 1 (highest) | boot | System initialization, non-interruptible |
| 2 | network | Connectivity critical for operation |
| 3 | call | User-facing real-time interaction |
| 4 | message | Notification |
| 5 | weather, clock, timer | Informational, can be deferred |
| 6 (lowest) | all other scenes | On-demand display |

Higher-priority scenes preempt lower-priority ones. Same-priority scenes
queue (FIFO). Emotions are always lowest priority and resume when no
scene is active.

---

## 13. Implementation Checklist

### Firmware Infrastructure (Done)
- [x] UART protocol: SYNC_DATA, OTA_STATUS, LOG_ENTRY, DEVICE_CMD message types
- [x] S3 StateTypes: ConnectionState(8), InteractionState(6), OtaState(8), EmotionState(16)
- [x] S3 StateManager: connection, OTA subscriptions
- [x] S3 UartBridge: SYNC_DATA, OTA_STATUS, DEVICE_CMD dispatch
- [x] C5 DataSyncManager: parse sync_data JSON, relay binary to S3
- [x] C5 WsMessageHandler: incoming WS message dispatcher
- [x] SceneData struct (in SceneManager.hpp)
- [x] VariantRegistry with category/variant registration
- [x] SceneManager lifecycle (show/exit/render)
- [x] DisplayManager: handleSyncData(), handleOtaStatus(), handleConnectivity()

### Rendering Engine (Phase B — Todo)
- [ ] GfxEngine core with PSRAM framebuffer
- [ ] Primitive rendering (rect, circle, line, arc, polygon, text)
- [ ] Transform stack, alpha blending
- [ ] High-level: drawEye, drawEyes, fillHeart, fillStar

### Boot + Network Scenes (Phase E1-E2 — Todo)
- [ ] Port 4 boot scene render functions
- [ ] Port 6 network scene render functions
- [ ] Wire ConnectionState changes to network scene triggers

### Time + Weather Scenes (Phase E3-E4 — Todo)
- [ ] Port 3 clock scene render functions
- [ ] Port 5 weather scene render functions
- [ ] Status bar live time display

### Remaining Scenes + Emotions (Phase D + E — Todo)
- [ ] Port 37 emotion categories (168 variants)
- [ ] Port 21 scene categories (59 variants)
- [ ] Scene preemption logic
