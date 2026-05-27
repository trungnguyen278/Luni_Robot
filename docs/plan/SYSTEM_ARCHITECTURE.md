# Luni Robot — System Architecture (Unified Plan)

> Version: 2.0 | Date: 2025-05-27
> Mục tiêu: Hệ thống self-hosted tối ưu, dễ mở rộng, expose qua Cloudflare Tunnel (free).

---

## 1. Tổng quan hệ thống

```
                        ┌─────────────────────────┐
                        │   Cloudflare (Free)      │
                        │                          │
                        │  Tunnel ← expose HTTPS   │
                        │  R2    ← OTA firmware    │
                        │  DNS   ← domain routing  │
                        └────────────┬─────────────┘
                                     │ HTTPS + WSS
                                     │
          ┌──────────────────────────┼──────────────────────────┐
          │                          │                          │
          │              ┌───────────▼───────────┐              │
          │              │       Nginx           │              │
          │              │   (Reverse Proxy)     │              │
          │              └───┬───────────┬───────┘              │
          │                  │           │                      │
          │      ┌───────────▼──┐  ┌─────▼──────────┐          │
          │      │   FastAPI    │  │    Next.js      │          │
          │      │   (Backend)  │  │    (Frontend)   │          │
          │      │              │  │                 │          │
          │      │  REST API    │  │  Web Admin      │          │
          │      │  WebSocket   │  │  User Dashboard │          │
          │      │  Background  │  │                 │          │
          │      └──┬────┬──────┘  └─────────────────┘          │
          │         │    │                                      │
          │    ┌────▼┐ ┌─▼─────┐                               │
          │    │ PG  │ │ Redis │                                │
          │    │(DB) │ │(Cache)│                                │
          │    └─────┘ └───────┘          Docker Compose        │
          └─────────────────────────────────────────────────────┘
                    │              │
               WebSocket      HTTP REST
               (Device)       (App/Web)
                    │              │
          ┌────────▼──┐    ┌──────▼──────┐
          │ Luni Robot │    │ Flutter App │
          │ (ESP32)    │    │ (iOS/Android)│
          └────────────┘    └─────────────┘
```

## 2. Nguyên tắc thiết kế

| # | Nguyên tắc | Lý do |
|---|-----------|-------|
| 1 | **Self-hosted + Cloudflare Tunnel** | Free, không phụ thuộc serverless. Tunnel proxy HTTPS+WSS. |
| 2 | **FastAPI backend + Next.js frontend** | Python dễ tích hợp AI/ML. Next.js SSR + React ecosystem. |
| 3 | **PostgreSQL = single source of truth** | App, Web, Server đều dùng 1 DB qua cùng 1 API. |
| 4 | **HTTP cho OTA, WS cho realtime** | WS text frame = JSON command, binary frame = audio. HTTP = firmware download. |
| 5 | **Docker Compose** | Tất cả services container hóa, deploy 1 lệnh, dễ migrate server. |
| 6 | **Structured logging + log levels** | Server, device, app đều có log level thống nhất. |
| 7 | **Role-based access** | Admin / User tách biệt, cùng API nhưng khác quyền. |

## 3. Communication Protocol

### 3.1 WebSocket Message Format (Text Frames)

```json
{
  "type": "<message_type>",
  "id": "uuid-v4",
  "ts": 1716825600000,
  "payload": { ... }
}
```

- **`type`**: Tên cụ thể của message (xem bảng dưới). Không dùng category chung.
- **`id`**: UUID v4 — dùng để match ACK / track request.
- **`ts`**: Epoch milliseconds.
- **`payload`**: Nội dung tùy theo `type`.

#### Auth Messages (Handshake)

| Direction | Type | Payload | Mô tả |
|-----------|------|---------|--------|
| Device → Server | `auth` | `{ "device_token": "hex128", "mac": "AA:BB:CC:DD:EE:FF", "fw_version": "2.1.0", "model": "luni_v2_s3c5" }` | First message sau WS open. Server verify token. |
| Server → Device | `auth_result` | `{ "status": "ok" }` hoặc `{ "status": "fail", "reason": "invalid_token" }` | Kết quả xác thực. Fail → server disconnect sau 1s. |

#### Command Types (Server → Device)

| Type | Payload | Mô tả |
|------|---------|--------|
| `set_volume` | `{ "value": 0-100 }` | Chỉnh âm lượng |
| `set_brightness` | `{ "value": 0-100 }` | Chỉnh độ sáng |
| `set_emotion` | `{ "emotion": "happy", "variant": "dancing" }` | Đổi cảm xúc |
| `set_scene` | `{ "scene": "weather", "data": {...} }` | Đổi scene + data |
| `reboot` | `{}` | Khởi động lại |
| `ota_available` | `{ "version": "2.1.0", "url": "https://...", "sha256": "...", "size": ... }` | Thông báo OTA mới |
| `sync_data` | `{ "time": ..., "weather": ..., "calendar": ... }` | Đồng bộ dữ liệu hiển thị |
| `tts_play` | `{ "text": "..." }` | Text-to-speech |
| `audio_stop` | `{}` | Dừng phát audio |
| `config_update` | `{ "key": "...", "value": "..." }` | Cập nhật cấu hình |
| `interaction_msg` | `{ "from": "user_id", "text": "...", "source": "app|web" }` | Tin nhắn từ user |
| `ack` | `{ "ref_id": "uuid of original msg" }` | Xác nhận đã nhận message |

#### Status Types (Device → Server)

| Type | Payload | Mô tả |
|------|---------|--------|
| `device_info` | `{ "mac", "fw_version", "model", "uptime", ... }` | Thông tin thiết bị (gửi sau auth OK) |
| `state_update` | `{ "category": "interaction|connectivity|power|emotion", "old": N, "new": N }` | Trạng thái thay đổi |
| `battery` | `{ "voltage", "percent", "charging" }` | Pin |
| `ota_progress` | `{ "percent", "phase": "download|verify|flash" }` | Tiến trình OTA |
| `error` | `{ "code", "message", "severity" }` | Lỗi |
| `log` | `{ "level": "debug|info|warn|error", "tag", "message" }` | Log từ device |
| `heartbeat` | `{ "uptime", "free_heap", "rssi" }` | Health check (mỗi 30s) |

#### Server → App/Web (qua WS `/ws/app/{device_id}`)

App/Web nhận 2 loại message qua WS app endpoint:
- **Relayed**: `state_update`, `ota_progress`, `error`, `battery` — server forward từ device
- **Server-generated**: `device_online`, `device_offline`, `interaction_result`, `current_state`

| Type | Payload | Mô tả |
|------|---------|--------|
| `device_online` | `{}` | Device vừa connect |
| `device_offline` | `{ "last_seen": "..." }` | Device mất kết nối |
| `current_state` | `{ "is_online", "emotion", "battery", ... }` | Gửi khi app connect (snapshot) |
| `interaction_result` | `{ "input": "...", "output": "...", "emotion": "..." }` | Kết quả LLM sau khi xử lý tương tác |

### 3.2 WebSocket Binary Frames (Audio)

```
┌─────────┬──────────┬──────────┬─────────────────┐
│ Header  │ Sequence │  Length  │   Opus Payload   │
│  0xAA   │  2 bytes │  2 bytes │   N bytes        │
│ (1 byte)│  (LE)    │  (LE)    │                  │
└─────────┴──────────┴──────────┴─────────────────┘
```

- **Direction bit**: `0xAA` = uplink (mic → server), `0xAB` = downlink (server → speaker)
- **Sequence**: Phát hiện mất gói, reorder
- **Opus**: 48kHz, 16-bit mono, frame 20ms

### 3.3 WS Auth Handshake Protocol

```
Device                           Server
  │                                │
  │── WS CONNECT /ws/device ──────▶│
  │◀──────── WS OPEN (101) ───────│
  │                                │
  │── TEXT: { type: "auth",        │
  │     payload: {                 │
  │       device_token: "hex128",  │  Timeout: 5s sau WS open
  │       mac: "AA:BB:...",        │  Nếu không nhận auth → disconnect
  │       fw_version: "2.1.0",    │
  │       model: "luni_v2_s3c5"   │
  │     }                          │
  │   } ──────────────────────────▶│
  │                                │── Lookup device_token in DB
  │                                │── Verify mac matches
  │                                │── Mark device online
  │◀── TEXT: { type: "auth_result",│
  │     payload: { status: "ok" }  │
  │   } ──────────────────────────│
  │                                │── Push sync_data immediately
  │── TEXT: { type: "device_info", │
  │     ... } ────────────────────▶│── Update DB (fw_version, etc.)
  │                                │
  │── TEXT: { type: "heartbeat" }──▶│  Mỗi 30s
  │       ...                      │  Server: mark offline nếu miss 3 lần (>90s)
```

**Auth failure:**
```
  │◀── TEXT: { type: "auth_result",
  │     payload: {
  │       status: "fail",
  │       reason: "invalid_token"  // hoặc "device_not_found", "token_mismatch"
  │     }
  │   } ──────────────────────────│
  │◀──────── WS CLOSE 4001 ──────│  (close sau 1s delay)
```

**Heartbeat timing:**
| Parameter | Value | Mô tả |
|-----------|-------|--------|
| Device heartbeat interval | 30s | Device gửi `heartbeat` message |
| Server check interval | 60s | Server scan tất cả connections |
| Offline threshold | 90s | 3 heartbeat missed → mark offline |
| WS auth timeout | 5s | Sau WS open, phải nhận `auth` trong 5s |
| WS ping/pong (transport) | 30s | WebSocket protocol-level keepalive |
| Reconnect backoff | 1s → 2s → 4s → ... → 30s max | Exponential backoff, max 10 retries |

### 3.4 OTA via HTTP

```
GET  /api/v1/ota/check?device_id={mac}&current_version={ver}
     → { "available": true, "version": "2.1.0", "url": "...", "sha256": "...", "size": ... }

GET  /api/v1/ota/download/{firmware_id}
     → Redirect to Cloudflare R2 presigned URL (hoặc stream từ local storage)
```

## 4. State Machine (Thống nhất)

### 4.1 Device Connection Lifecycle

```
                    ┌──────────┐
         boot ──────│ OFFLINE  │◄──── max retries (server fail)
                    └─────┬────┘
                          │ có credentials?
                    ┌─────▼──────┐      không
                    │ WIFI_CONN  │──────────────────→ BLE_PROVISION
                    └─────┬──────┘                        │
                          │ WiFi OK + got IP              │ credentials saved
                    ┌─────▼──────┐                        │
                    │ WS_CONN    │◄───────────────────────┘
                    └─────┬──────┘
                          │ fail?
                          ├── DNS fail     → RECONNECT (no internet)
                          ├── TCP refused  → RECONNECT (server down)
                          ├── TLS error    → RECONNECT (cert issue)
                          │
                          │ WS open OK
                    ┌─────▼──────┐
                    │ WS_AUTH    │
                    └─────┬──────┘
                          │ fail?
                          ├── timeout      → RECONNECT
                          ├── 4001 reject  → BLE_PROVISION (re-pair)
                          │
                          │ auth OK
                    ┌─────▼──────┐
                    │  ONLINE    │
                    └─────┬──────┘
                          │ disconnect
                    ┌─────▼──────────────────────────────────────┐
                    │ RECONNECT                                   │
                    │                                             │
                    │ fail_reason decides:                        │
                    │  WIFI_AUTH_FAIL / AUTH_REJECTED → BLE_PROV  │
                    │  DNS_FAIL (5x)                 → BLE_PROV  │
                    │  SERVER_UNREACHABLE (10x)       → OFFLINE   │
                    │  WiFi lost                      → WIFI_CONN │
                    │  WiFi OK                        → WS_CONN   │
                    └────────────────────────────────────────────┘
```

**3 trường hợp lỗi chính:**

| Trường hợp | Phát hiện bằng | Fail reason | Hành xử |
|---|---|---|---|
| Không có WiFi | `WIFI_CONNECTING` fail | `WIFI_NOT_FOUND` / `WIFI_AUTH_FAIL` | Retry 3x → BLE pair lại |
| Có WiFi, không internet | DNS resolve fail | `DNS_FAIL` | Retry 5x → BLE pair lại (sai mạng?) |
| Có internet, không server | TCP connect refused/timeout | `SERVER_UNREACHABLE` | Retry 10x (exp backoff) → OFFLINE |

### 4.2 Interaction State Machine

```
                    ┌──────────┐
                    │   IDLE   │◄────────────────────────────────┐
                    └─────┬────┘                                 │
                          │ trigger (button/wake-word/app)       │
                    ┌─────▼──────┐                               │
                    │ TRIGGERED  │                               │
                    └─────┬──────┘                               │
                          │ WS audio stream start                │
                    ┌─────▼──────┐                               │
                    │ LISTENING  │──── silence/cancel ────┐      │
                    └─────┬──────┘                        │      │
                          │ end-of-speech                 │      │
                    ┌─────▼──────┐                        │      │
                    │ PROCESSING │                        │      │
                    └─────┬──────┘                        │      │
                          │ TTS audio start               │      │
                    ┌─────▼──────┐                        │      │
                    │  SPEAKING  │                        │      │
                    └─────┬──────┘                        │      │
                          │ audio complete                │      │
                          ▼                               ▼      │
                       IDLE ◄──────────────── CANCELLING ───────┘
```

### 4.3 OTA State Machine

```
    IDLE ──→ CHECK ──→ DOWNLOAD ──→ VERIFY ──→ FLASH ──→ REBOOT
              │            │           │          │
              ▼            ▼           ▼          ▼
           NO_UPDATE    DL_ERROR    BAD_HASH   FLASH_ERROR
              │            │           │          │
              └────────────┴───────────┴──────────┘
                               │
                            ROLLBACK → IDLE
```

## 5. Data Flow — Thông tin hiển thị UI

```
┌──────────────────┐        ┌──────────────────┐        ┌──────────────────┐
│   External APIs  │        │   FastAPI Server  │        │   Luni Robot     │
│                  │        │   (Background)    │        │                  │
│  • OpenWeather   │───────▶│  Fetch & Cache    │───────▶│  Display Scene   │
│  • WorldTime API │        │  in Redis         │        │  render_weather  │
│  • Calendar API  │        │  TTL: 15-60 min   │        │  render_clock    │
│  • Lunar calendar│        │                   │        │  render_calendar │
│  • News API      │        │  On WS connect:   │        │                  │
│                  │        │  push sync_data   │        │  SceneManager    │
└──────────────────┘        │  to device        │        │  lưu vào RAM     │
                            └──────────────────┘        └──────────────────┘
```

**`sync_data` payload:**

```json
{
  "time": {
    "unix": 1716825600,
    "tz": "Asia/Ho_Chi_Minh",
    "utc_offset": 7
  },
  "weather": {
    "temp": 32,
    "feels_like": 36,
    "humidity": 75,
    "condition": "partly_cloudy",
    "icon": "02d",
    "aqi": 85,
    "forecast": [
      { "day": "wed", "high": 34, "low": 26, "condition": "rain" }
    ]
  },
  "calendar": {
    "lunar": { "day": 1, "month": 5, "year": "Ất Tỵ" },
    "events": [
      { "title": "Meeting", "start": "2025-05-28T09:00:00+07:00" }
    ]
  },
  "location": {
    "city": "Hà Nội",
    "lat": 21.0285,
    "lon": 105.8542
  }
}
```

## 6. Database Schema (PostgreSQL — Unified)

> **Tất cả client (App, Web, Device) đều truy cập qua FastAPI REST API.**
> Không client nào connect trực tiếp DB.

```sql
-- ==========================================
-- USERS & AUTH
-- ==========================================

CREATE TABLE users (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  email       VARCHAR(255) UNIQUE NOT NULL,
  password    VARCHAR(255) NOT NULL,     -- bcrypt hash
  name        VARCHAR(100) NOT NULL,
  role        VARCHAR(20) NOT NULL DEFAULT 'user',  -- 'admin' | 'user'
  avatar_url  TEXT,
  is_active   BOOLEAN DEFAULT true,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  last_login  TIMESTAMPTZ
);

CREATE TABLE refresh_tokens (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  token       VARCHAR(512) UNIQUE NOT NULL,
  device_info TEXT,                       -- "Flutter App / Android 14"
  expires_at  TIMESTAMPTZ NOT NULL,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ==========================================
-- DEVICES
-- ==========================================

CREATE TABLE devices (
  id          VARCHAR(17) PRIMARY KEY,    -- MAC address "AA:BB:CC:DD:EE:FF"
  owner_id    UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  name        VARCHAR(100) NOT NULL DEFAULT 'Luni',
  model       VARCHAR(50) NOT NULL,       -- 'luni_v2_s3c5'
  fw_version  VARCHAR(20),
  hw_version  VARCHAR(20),
  location    VARCHAR(100),               -- "Phòng khách"
  timezone    VARCHAR(50) DEFAULT 'Asia/Ho_Chi_Minh',
  latitude    DOUBLE PRECISION,
  longitude   DOUBLE PRECISION,
  city        VARCHAR(100),

  -- Config JSON
  config      JSONB NOT NULL DEFAULT '{
    "volume": 60,
    "brightness": 100,
    "log_level": "info",
    "auto_ota": false,
    "sleep_schedule": null
  }',

  -- Auth
  device_token VARCHAR(128) NOT NULL,     -- pre-shared token for WS auth

  -- Status (updated by WS session)
  is_online   BOOLEAN DEFAULT false,
  last_state  JSONB,                      -- last known state snapshot
  last_seen   TIMESTAMPTZ,

  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  updated_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- Chia sẻ thiết bị
CREATE TABLE device_shares (
  device_id   VARCHAR(17) NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  user_id     UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
  permission  VARCHAR(20) NOT NULL DEFAULT 'control',  -- 'view' | 'control'
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (device_id, user_id)
);

-- ==========================================
-- LOGS (device + server)
-- ==========================================

-- Partitioned by month for performance
CREATE TABLE device_logs (
  id          BIGSERIAL,
  device_id   VARCHAR(17) NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  source      VARCHAR(10) NOT NULL DEFAULT 'device',  -- 'device' | 'server'
  level       VARCHAR(10) NOT NULL,       -- 'debug' | 'info' | 'warn' | 'error'
  tag         VARCHAR(50) NOT NULL,       -- module name: "WiFi", "Audio", "OTA"
  message     TEXT NOT NULL,
  metadata    JSONB,                      -- extra structured data
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  PRIMARY KEY (id, created_at)
) PARTITION BY RANGE (created_at);

-- Auto-create monthly partitions (via pg_partman or manual)
-- Retention: 90 ngày, sau đó auto-drop partition cũ

CREATE TABLE server_logs (
  id          BIGSERIAL PRIMARY KEY,
  level       VARCHAR(10) NOT NULL,
  module      VARCHAR(100) NOT NULL,      -- "api.auth", "ws.device", "task.weather"
  message     TEXT NOT NULL,
  metadata    JSONB,
  request_id  UUID,                       -- trace correlation
  user_id     UUID,
  device_id   VARCHAR(17),
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ==========================================
-- STATISTICS
-- ==========================================

CREATE TABLE usage_stats (
  device_id    VARCHAR(17) NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  date         DATE NOT NULL,
  interactions INTEGER DEFAULT 0,
  audio_secs   INTEGER DEFAULT 0,
  uptime_secs  INTEGER DEFAULT 0,
  errors       INTEGER DEFAULT 0,
  warnings     INTEGER DEFAULT 0,
  avg_rssi     SMALLINT,
  min_battery  SMALLINT,
  PRIMARY KEY (device_id, date)
);

-- ==========================================
-- OTA / FIRMWARE
-- ==========================================

CREATE TABLE firmware (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  version     VARCHAR(20) NOT NULL,
  model       VARCHAR(50) NOT NULL,       -- 'luni_s3' | 'luni_c5'
  storage_url TEXT NOT NULL,              -- R2 URL or local path
  sha256      VARCHAR(64) NOT NULL,
  size        INTEGER NOT NULL,
  changelog   TEXT,
  channel     VARCHAR(20) DEFAULT 'stable',  -- 'stable' | 'beta' | 'dev'
  is_active   BOOLEAN DEFAULT true,
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE(version, model)
);

CREATE TABLE ota_history (
  id          BIGSERIAL PRIMARY KEY,
  device_id   VARCHAR(17) NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  firmware_id UUID NOT NULL REFERENCES firmware(id),
  status      VARCHAR(20) NOT NULL,       -- 'started' | 'downloading' | 'flashing' | 'completed' | 'failed'
  progress    SMALLINT DEFAULT 0,
  error       TEXT,
  started_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
  completed_at TIMESTAMPTZ
);

-- ==========================================
-- INTERACTIONS (App/Web ↔ Robot)
-- ==========================================

CREATE TABLE interactions (
  id          BIGSERIAL PRIMARY KEY,
  device_id   VARCHAR(17) NOT NULL REFERENCES devices(id) ON DELETE CASCADE,
  user_id     UUID REFERENCES users(id),  -- NULL = voice trigger from device
  direction   VARCHAR(10) NOT NULL,       -- 'user_to_device' | 'device_to_user' | 'voice'
  source      VARCHAR(10) NOT NULL,       -- 'app' | 'web' | 'voice' | 'button'
  input_text  TEXT,                       -- STT result or typed text
  output_text TEXT,                       -- LLM response
  emotion     VARCHAR(30),               -- emotion triggered
  audio_secs  REAL,
  latency_ms  INTEGER,                    -- processing time
  created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ==========================================
-- INDEXES
-- ==========================================

CREATE INDEX idx_logs_device_level ON device_logs(device_id, level, created_at DESC);
CREATE INDEX idx_logs_created ON device_logs(created_at DESC);
CREATE INDEX idx_server_logs_level ON server_logs(level, created_at DESC);
CREATE INDEX idx_server_logs_module ON server_logs(module, created_at DESC);
CREATE INDEX idx_server_logs_request ON server_logs(request_id) WHERE request_id IS NOT NULL;
CREATE INDEX idx_stats_date ON usage_stats(date DESC);
CREATE INDEX idx_ota_device ON ota_history(device_id, started_at DESC);
CREATE INDEX idx_interactions_device ON interactions(device_id, created_at DESC);
CREATE INDEX idx_interactions_user ON interactions(user_id, created_at DESC);
CREATE INDEX idx_refresh_tokens_user ON refresh_tokens(user_id);
CREATE INDEX idx_refresh_tokens_expires ON refresh_tokens(expires_at);
```

## 7. Authentication & Security

```
┌───────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   App/Web     │     │   FastAPI        │     │   Device        │
│               │     │   Server         │     │                 │
│  Login ───────┼────▶│  POST /auth/login│     │                 │
│  → JWT token  │◄────┤  → JWT (1h)      │     │                 │
│               │     │  + Refresh (30d)  │     │                 │
│               │     │                  │     │                 │
│  API calls    │────▶│  Verify JWT      │     │                 │
│  Auth: Bearer │     │  Check role      │     │                 │
│               │     │                  │     │                 │
│  WS connect   │────▶│  WS /ws/app      │     │                 │
│  + JWT token  │     │  Verify → relay  │     │                 │
│               │     │                  │     │  WS /ws/device  │
│               │     │                  │◄────┤  + device_token │
│               │     │  Verify token    │     │  (pre-shared)   │
│               │     │  → session       │────▶│  ACK            │
└───────────────┘     └──────────────────┘     └─────────────────┘
```

- **App/Web**: JWT access token (1h) + refresh token (30d, stored in DB)
- **Device**: Pre-shared device token (provisioned via BLE), rotatable from admin
- **Admin BLE**: `admin_secret = HMAC-SHA256(device_token, SERVER_SECRET_KEY)` — server derives, app writes qua BLE during pairing. Dùng để verify admin BLE token (Level 2).
- **API**: All endpoints require auth, role check via FastAPI dependency injection
- **WebSocket**: 2 endpoints — `/ws/device` (device_token) + `/ws/app/{device_id}` (JWT)

## 8. Logging System (Thống nhất)

### Log Levels — tất cả components

| Level | Số | Robot (ESP32) | Server (FastAPI) | App (Flutter) |
|-------|----|--------------|-----------------|---------------|
| `DEBUG` | 10 | SPI frames, codec stats, heap | DB queries, WS frames, cache | Network debug |
| `INFO` | 20 | State changes, boot, OTA OK | API requests, connections | User actions |
| `WARN` | 30 | Low battery, WiFi unstable | Rate limit hit, slow queries | Retry, timeout |
| `ERROR` | 40 | Hardware fault, OTA fail | Exceptions, WS errors | Crash, API fail |

### Server logging (Python structlog)

```python
import structlog

logger = structlog.get_logger()

# Mỗi request có request_id để trace
logger.info("device.connected",
    device_id="AA:BB:CC:DD:EE:FF",
    fw_version="2.1.0",
    request_id="uuid"
)

logger.error("ota.download_failed",
    device_id="AA:BB:CC:DD:EE:FF",
    firmware_version="2.1.0",
    error="timeout after 30s"
)
```

### Log level control

| Thay đổi | Cách làm |
|----------|---------|
| Server log level | `LOG_LEVEL` env var trong docker-compose, hoặc API admin endpoint |
| Device log level | `config.log_level` trong DB → push qua WS `config_update` |
| Per-device debug | Admin bật debug tạm cho 1 device → device gửi debug logs → server lưu |
| Log retention | Server: 90 ngày DB, archive older → file/R2. Cron job cleanup. |

## 9. Hệ thống tương tác (App/Web ↔ Robot)

### 9.1 Luồng tương tác

```
┌──────────┐        ┌──────────────┐        ┌──────────────┐
│  App/Web │        │   FastAPI    │        │  Luni Robot  │
│  User    │        │   Server     │        │              │
│          │        │              │        │              │
│ ── Text Input ──▶ │              │        │              │
│ "Hôm nay          │  LLM Process │        │              │
│  thời tiết        │  (Claude/    │        │              │
│  thế nào?"        │   GPT API)   │        │              │
│          │        │      │       │        │              │
│          │        │      ▼       │        │              │
│          │        │  Response    │        │              │
│          │        │  text + emotion       │              │
│          │        │      │       │        │              │
│          │        │      ├──────▶│  WS: tts_play        │
│          │        │      │       │  + set_emotion        │
│  ◀── Response ──  │      │       │        │              │
│  text + emotion   │              │  Robot speaks +       │
│          │        │              │  changes expression   │
└──────────┘        └──────────────┘        └──────────────┘

── Voice (Robot button) ──────────────────────────────────▶
│                                                          │
│  Robot mic → Opus → WS binary → Server                   │
│  Server: STT → LLM → TTS → WS binary → Robot speaker    │
│  Server: save interaction to DB                          │
│  Server: push result to App/Web WS if connected          │
```

### 9.2 Tính năng tương tác (tương lai)

| Feature | Mô tả | Priority |
|---------|--------|----------|
| **Text chat** | User gõ text từ app/web → robot nói + hiển thị emotion | P1 |
| **Voice relay** | User nói từ app → stream audio → robot phát | P2 |
| **Interaction history** | Xem lịch sử hội thoại trên app/web | P1 |
| **Scheduled messages** | Hẹn giờ robot nói 1 câu (nhắc nhở, alarm) | P2 |
| **Multi-user** | Nhiều user cùng tương tác 1 robot, thấy nhau | P3 |
| **Personality config** | Tùy chỉnh tính cách/giọng robot per-device | P2 |

## 10. Công nghệ chọn

| Layer | Công nghệ | Lý do |
|-------|-----------|-------|
| **Backend API** | FastAPI (Python 3.12) | Async native, WebSocket built-in, dễ tích hợp AI libs |
| **Frontend Web** | Next.js 14 (React) | SSR, App Router, deploy Cloudflare Pages hoặc Docker |
| **Database** | PostgreSQL 16 | JSONB, partitioning, full-text search, mature |
| **Cache / Pub-Sub** | Redis 7 | Session cache, weather cache, WS pub/sub giữa workers |
| **Reverse Proxy** | Nginx | WS upgrade, static files, load balancing, SSL termination |
| **Tunnel** | Cloudflare Tunnel (free) | Expose self-hosted HTTPS+WSS, DDoS protection, DNS |
| **OTA Storage** | Cloudflare R2 (free 10GB) | Firmware binary storage, S3-compatible |
| **Container** | Docker Compose | Dev + prod cùng config, dễ migrate, reproducible |
| **Mobile App** | Flutter | Cross-platform iOS/Android, single codebase |
| **AI/TTS/STT** | OpenAI API / Claude API | STT (Whisper), LLM, TTS — qua server proxy |
| **Task Queue** | APScheduler / Celery | Background: weather fetch, log cleanup, stats aggregate |
| **Server Logging** | structlog + PostgreSQL | Structured JSON logs, queryable, log levels |
| **Robot FW** | ESP-IDF + PlatformIO (C++17) | Giữ nguyên, đã proven stable |

## 11. Roadmap phát triển

### Phase 1 — Foundation (4-6 tuần)
- [ ] Server: Docker Compose (FastAPI + PostgreSQL + Redis + Nginx)
- [ ] Server: Auth system (JWT + refresh + role-based)
- [ ] Server: Device registry + WebSocket session manager
- [ ] Server: Cloudflare Tunnel setup
- [ ] Robot: Refactor NetworkManager — bỏ MQTT, WS-only + HTTP OTA
- [ ] Robot: Thống nhất state machine giữa S3 và C5

### Phase 2 — Core Features (4-6 tuần)
- [ ] Server: AI proxy (STT/TTS/LLM), audio streaming pipeline
- [ ] Server: sync_data system (weather, time, calendar) + Redis cache
- [ ] Server: Structured logging (structlog → PostgreSQL)
- [ ] Robot: Tích hợp sync_data vào SceneManager
- [ ] App: Flutter skeleton, auth, device pairing (BLE)

### Phase 3 — Management & Interaction (4-6 tuần)
- [ ] Web: Next.js dashboard, device management, log viewer
- [ ] Server: OTA management (upload → R2, deploy, rollout tracking)
- [ ] Server: Interaction system (text chat app↔robot, history)
- [ ] App: Device control, emotion/scene picker, chat, settings
- [ ] Server: Usage statistics aggregation (cron)

### Phase 4 — Polish (2-4 tuần)
- [ ] Server: Rate limiting, abuse protection
- [ ] App/Web: Push notifications (FCM)
- [ ] Web: Admin dashboard (users, firmware, system health)
- [ ] Robot: OTA flow hoàn chỉnh, error recovery
- [ ] Server: Log rotation, partition management, monitoring

---

> **Xem chi tiết:**
> - [PLAN_SERVER.md](https://github.com/trungnguyen278/Luni_Cloud/blob/main/docs/plan/PLAN_SERVER.md) — FastAPI server, Docker, logging, Next.js web frontend *(Luni_Cloud)*
> - [PLAN_APP.md](https://github.com/trungnguyen278/Luni_App/blob/main/docs/plan/PLAN_APP.md) — Flutter mobile app + BLE pairing flow *(Luni_App)*
> - [PLAN_ROBOT.md](PLAN_ROBOT.md) — Robot firmware refactoring *(repo này)*
