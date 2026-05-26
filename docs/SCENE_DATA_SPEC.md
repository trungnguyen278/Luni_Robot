# Scene System & Real-Time Data Specification

How scenes are registered, how they receive live data (time, weather,
network status), and how the boot/network state machines work.

---

## 1. Scene Category Registry

### 1.1 All 21 Scene Categories

| Category | Variants | Default Tone | Data Source | Real-time? |
|----------|----------|-------------|-------------|-----------|
| boot | 4 | red | Internal FSM | No |
| network | 6 | cyan | ConnectivityState | Yes |
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
// esp32-s3/src/ui/SceneData.hpp

struct SceneData {
    // --- Time (from NTP via C5) ---
    uint8_t  hours   = 0;       // 0-23
    uint8_t  minutes = 0;       // 0-59
    uint8_t  seconds = 0;       // 0-59
    uint8_t  day_of_week = 0;   // 0=Sun, 6=Sat
    uint8_t  day     = 1;       // 1-31
    uint8_t  month   = 1;       // 1-12
    uint16_t year    = 2026;
    bool     time_valid = false;

    // --- Weather (from OpenWeather API via C5) ---
    int16_t  weather_temp_c = 0;       // Temperature in Celsius
    uint8_t  weather_condition = 0;    // See WeatherCondition enum
    char     weather_desc[16] = {};    // "SUNNY", "RAINY", etc.
    bool     weather_valid = false;

    // --- Timer ---
    uint16_t timer_remaining_s = 0;
    uint16_t timer_total_s = 0;

    // --- Boot ---
    uint8_t  boot_progress_pct = 0;    // 0-100
    uint8_t  boot_check_results[5] = {}; // 0=pending, 1=ok, 2=fail
    // Indices: 0=DISPLAY, 1=AUDIO, 2=MIC, 3=NETWORK, 4=AI_CORE

    // --- Network ---
    char     ssid[33] = {};            // Connected/target SSID
    uint8_t  retry_attempt = 0;        // Current retry number
    uint8_t  retry_max = 3;
    uint16_t error_code = 0;           // HTTP/server error code

    // --- Music ---
    char     track_name[32] = {};
    float    music_progress = 0;       // 0.0 - 1.0

    // --- Fitness ---
    uint16_t heart_rate = 0;           // BPM
    uint32_t steps = 0;
    uint32_t step_goal = 10000;

    // --- Call ---
    char     caller_name[32] = {};
    uint8_t  call_type = 0;            // 0=incoming, 1=active, 2=missed

    // --- Message ---
    char     sender_name[32] = {};
    uint8_t  message_count = 0;

    // --- Navigation ---
    float    nav_bearing = 0;          // Degrees (0=N, 90=E)
    float    nav_distance_km = 0;
    char     nav_destination[32] = {};

    // --- Calendar ---
    char     event_name[32] = {};
    uint8_t  event_hour = 0;
    uint8_t  event_minute = 0;
};

enum class WeatherCondition : uint8_t {
    SUNNY   = 0,  // OpenWeather: 800
    CLOUDY  = 1,  // OpenWeather: 801-804
    RAINY   = 2,  // OpenWeather: 300-531
    SNOW    = 3,  // OpenWeather: 600-622
    STORM   = 4,  // OpenWeather: 200-232
};
```

---

## 5. Data Flow Architecture

```
┌──────────────────────────────────────────┐
│               ESP32-C5                    │
│  (WiFi + BLE + MQTT + API calls)         │
│                                           │
│  ┌─────────┐  ┌──────────────┐           │
│  │  NTP    │  │ OpenWeather  │           │
│  │  Client │  │ HTTP Client  │           │
│  └────┬────┘  └──────┬───────┘           │
│       │              │                    │
│       ▼              ▼                    │
│  ┌──────────────────────────────┐        │
│  │  UartBridge (TX to S3)      │        │
│  │  TIME_SYNC / WEATHER_UPDATE │        │
│  │  STATUS_UPDATE (existing)   │        │
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
│  │  + TimeState (new)           │        │
│  │  + WeatherState (new)        │        │
│  │  + ConnectivityState (exists)│        │
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

### 6.1 New Message Types

Add to `MsgType` enum:

```cpp
enum class MsgType : uint8_t {
    STATUS_UPDATE  = 0x01,  // existing: C5 → S3 state snapshot
    CONTROL_CMD    = 0x02,  // existing: S3 → C5 command
    TIME_SYNC      = 0x03,  // new: C5 → S3 time data
    WEATHER_UPDATE = 0x04,  // new: C5 → S3 weather data
    SCENE_TRIGGER  = 0x05,  // new: C5 → S3 show specific scene
};
```

### 6.2 TIME_SYNC Payload (7 bytes)

```
Byte 0: hours    (0-23)
Byte 1: minutes  (0-59)
Byte 2: seconds  (0-59)
Byte 3: day_of_week (0-6, 0=Sunday)
Byte 4: day      (1-31)
Byte 5: month    (1-12)
Byte 6-7: year   (uint16_t LE, e.g. 2026)
```

C5 sends this every 60 seconds after NTP sync, or immediately on first
sync after boot.

### 6.3 WEATHER_UPDATE Payload (variable, max 20 bytes)

```
Byte 0:   condition (WeatherCondition enum: 0-4)
Byte 1-2: temp_c    (int16_t LE, in Celsius)
Byte 3:   desc_len  (length of description string)
Byte 4+:  desc      (ASCII string, e.g. "SUNNY", max 16 chars)
```

C5 sends this every 10 minutes from OpenWeather API, or on demand.

### 6.4 SCENE_TRIGGER Payload (variable)

```
Byte 0:   category_id_len
Byte 1+:  category_id  (ASCII, e.g. "weather")
Byte N:   variant_id_len (0 = auto-pick)
Byte N+1: variant_id   (ASCII, e.g. "sunny")
```

Allows the server/C5 to trigger specific scenes on the display.

### 6.5 Updated StatusPayload

The existing `StatusPayload` stays unchanged. The new message types are
separate frames, not extensions of STATUS_UPDATE.

---

## 7. StateManager Extension

### 7.1 New State Types

Add to `StateTypes.hpp`:

```cpp
// Time state (populated by NTP via C5)
struct TimeState {
    uint8_t  hours = 0;
    uint8_t  minutes = 0;
    uint8_t  seconds = 0;
    uint8_t  day_of_week = 0;
    uint8_t  day = 1;
    uint8_t  month = 1;
    uint16_t year = 2026;
    bool     valid = false;
    uint32_t last_sync_ms = 0;  // millis() of last NTP sync
};

// Weather state (populated by OpenWeather API via C5)
struct WeatherState {
    int16_t  temp_c = 0;
    uint8_t  condition = 0;     // WeatherCondition enum
    char     desc[16] = {};
    bool     valid = false;
    uint32_t last_update_ms = 0;
};
```

### 7.2 Expanded EmotionState

The current `EmotionState` enum has only 8 values. It needs expansion to
cover all 37 emotion categories:

```cpp
enum class EmotionState : uint8_t {
    NEUTRAL = 0,
    HAPPY, SAD, ANGRY, CONFUSED, EXCITED, CALM, THINKING,
    // v2.0 additions:
    GREET, BORED, LOVE, SHY, EMBARRASSED, WINK, SMUG, PROUD,
    COOL, MISCHIEVOUS, SUSPICIOUS, CURIOUS, ANNOYED, NERVOUS,
    SLEEPY, SLEEPING, HUNGRY, CRYING, DISGUSTED, FOCUSED,
    DETERMINED, LOADING, CHARGING, DIZZY, DEAD, ERROR, MUTE,
    SCARED, SURPRISED,
    COUNT
};
```

The C5 sends the expanded emotion ID in `StatusPayload.emotion`. The S3
maps it to a category key via lookup table.

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
  │ PTIT mark fade-in + sweep ring
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

### 9.2 Mapping ConnectivityState to Scene Variants

```cpp
void DisplayManager::handleConnectivity(ConnectivityState state) {
    switch (state) {
    case ConnectivityState::OFFLINE:
        scene_mgr_->showScene("network", "network-offline");
        break;
    case ConnectivityState::CONNECTING_WIFI:
        scene_mgr_->showScene("network", "network-wifi-connect");
        break;
    case ConnectivityState::WIFI_CONNECTED:
        // Brief wifi-scan display, then proceed to WS connection
        break;
    case ConnectivityState::CONNECTING_WS:
        // Show connecting indicator or keep wifi-connect
        break;
    case ConnectivityState::ONLINE:
        scene_mgr_->exitScene(); // Return to emotion idle loop
        break;
    }
}
```

### 9.3 Network Scenes Block Emotions

While any `network-*` scene is playing, the emotion system is suspended.
User-facing emotions should not interrupt connectivity status displays.

---

## 10. NTP Time Integration

### 10.1 ESP32-C5 Side

```cpp
// After WiFi connected:
void initNTP() {
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

// Periodic sync task (every 60s):
void ntpSyncTask() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    uint8_t payload[8];
    payload[0] = timeinfo.tm_hour;
    payload[1] = timeinfo.tm_min;
    payload[2] = timeinfo.tm_sec;
    payload[3] = timeinfo.tm_wday;
    payload[4] = timeinfo.tm_mday;
    payload[5] = timeinfo.tm_mon + 1;
    payload[6] = (timeinfo.tm_year + 1900) & 0xFF;
    payload[7] = ((timeinfo.tm_year + 1900) >> 8) & 0xFF;

    uart_bridge.send(MsgType::TIME_SYNC, payload, 8);
}
```

### 10.2 ESP32-S3 Side

```cpp
// In UartBridge RX handler:
case MsgType::TIME_SYNC:
    if (len >= 8) {
        TimeState ts;
        ts.hours       = payload[0];
        ts.minutes     = payload[1];
        ts.seconds     = payload[2];
        ts.day_of_week = payload[3];
        ts.day         = payload[4];
        ts.month       = payload[5];
        ts.year        = payload[6] | (payload[7] << 8);
        ts.valid       = true;
        ts.last_sync_ms = millis();
        state_mgr_->setTimeState(ts);
    }
    break;
```

Between syncs, S3 increments seconds locally using elapsed millis.

### 10.3 Status Bar Time Display

The status bar always shows current time (top-left, cyan, 11px monospace):

```cpp
void StatusBar::render(GfxEngine& gfx, const TimeState& time) {
    char buf[6]; // "HH:MM"
    snprintf(buf, sizeof(buf), "%02d:%02d", time.hours, time.minutes);
    gfx.drawText(buf, 8, 14, FONT_11, TONE_LUT[TONE_CYAN],
                 TextAlign::LEFT, 217); // 85% opacity
}
```

---

## 11. Weather API Integration

### 11.1 ESP32-C5 Side

```cpp
// HTTP GET every 10 minutes:
// https://api.openweathermap.org/data/2.5/weather?lat={lat}&lon={lon}&appid={key}&units=metric

void weatherTask() {
    // Parse JSON response:
    // - temp: main.temp (float → int16_t)
    // - condition: weather[0].id → WeatherCondition enum
    //   200-232 → STORM, 300-531 → RAINY, 600-622 → SNOW,
    //   800 → SUNNY, 801-804 → CLOUDY
    // - desc: weather[0].main (uppercase, max 16 chars)

    uint8_t payload[20];
    payload[0] = condition;
    int16_t temp = (int16_t)roundf(main_temp);
    memcpy(&payload[1], &temp, 2);
    uint8_t desc_len = strlen(desc);
    payload[3] = desc_len;
    memcpy(&payload[4], desc, desc_len);

    uart_bridge.send(MsgType::WEATHER_UPDATE, payload, 4 + desc_len);
}
```

### 11.2 OpenWeather Condition Code Mapping

| OpenWeather ID | WeatherCondition | Scene Variant | Tone |
|---------------|-----------------|---------------|------|
| 200-232 | STORM | weather-storm | purple |
| 300-531 | RAINY | weather-rainy | blue |
| 600-622 | SNOW | weather-snow | white |
| 800 | SUNNY | weather-sunny | warm |
| 801-804 | CLOUDY | weather-cloudy | cyan |

### 11.3 Weather Scene Rendering

Weather scenes are data-driven — they read temperature and condition from
`SceneData`:

```cpp
void render_weather_sunny(GfxEngine& gfx, float t, const ColorContext& colors) {
    auto& data = SceneManager::instance().getSceneData();

    // Sun icon with rotating rays (8 lines)
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
    snprintf(temp, sizeof(temp), "%d\xC2\xB0", data.weather_temp_c);
    gfx.drawText(temp, SCX + 60, cy - 4, FONT_36, colors.eye, TextAlign::CENTER);
    gfx.drawText(data.weather_desc, SCX + 60, cy + 26, FONT_12,
                 colors.eye, TextAlign::CENTER, 178);
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

### Phase 1: Core infrastructure
- [ ] Add `MsgType::TIME_SYNC`, `WEATHER_UPDATE`, `SCENE_TRIGGER` to UartProtocol.hpp (both C5 and S3)
- [ ] Add `TimeState`, `WeatherState` to StateTypes.hpp
- [ ] Extend `StateManager` with `setTimeState()`, `setWeatherState()`
- [ ] Extend `UartBridge` RX handler for new message types
- [ ] Implement `SceneData` struct
- [ ] Implement `VariantRegistry` with category/variant registration
- [ ] Implement `SceneManager` lifecycle (show/exit/render)

### Phase 2: Boot + Network
- [ ] Implement boot FSM in AppController
- [ ] Port 4 boot scene render functions from `scenes-boot.jsx`
- [ ] Port 6 network scene render functions from `scenes-network.jsx`
- [ ] Wire `ConnectivityState` changes to network scene triggers
- [ ] Port shared glyphs: `WifiIcon`, `BTIcon`, `CloudIcon`, `MonoLabel`

### Phase 3: Time + Weather
- [ ] Implement NTP client on C5
- [ ] Implement OpenWeather HTTP client on C5
- [ ] Send TIME_SYNC and WEATHER_UPDATE via UART
- [ ] Port 3 clock scene render functions
- [ ] Port 5 weather scene render functions
- [ ] Wire status bar time display

### Phase 4: Remaining scenes
- [ ] Port scene categories in priority order (music, timer, fitness, etc.)
- [ ] Implement SCENE_TRIGGER from server for on-demand scene display
- [ ] Add scene preemption logic
