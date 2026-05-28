# Luni UI System — Progress Tracker

> Chuyển đổi 227 UI variants từ Claude Design (JSX/SVG) sang procedural C++ rendering trên ESP32-S3 N16R8.

**Board target**: ESP32-S3 N16R8 (16MB flash, 8MB PSRAM)
**Display**: ST7789 320x240 RGB565 SPI 40MHz
**Design source**: `ui_design/` — 58 categories, 227 variants, 9-tone palette

---

## Phase A: Documentation

| # | Task | Status | Ghi chú |
|---|------|--------|---------|
| A1 | `docs/UI_DESIGN_PIPELINE.md` | Done | JSX→C++ mapping, color porting, step-by-step |
| A2 | `docs/RENDERING_ARCHITECTURE.md` | Done | GfxEngine API, framebuffer, performance |
| A3 | `docs/SCENE_DATA_SPEC.md` | Done | Scene registry, data flow, boot/network FSM |
| A4 | `platformio.ini` chuyển sang N16R8 + `partitions_16MB.csv` | Todo | Board hiện tại vẫn N4R8 |

---

## Phase B: GfxEngine — Rendering Engine

| # | Task | Status | File(s) | Ghi chú |
|---|------|--------|---------|---------|
| B1 | MathHelpers | Todo | `lib/display/MathHelpers.hpp` | lerp, ease, pulse, blink, sin LUT |
| B2 | ColorSystem | Todo | `lib/display/ColorSystem.hpp` | ToneId, TONE_LUT, ColorContext |
| B3 | GfxEngine core | Todo | `lib/display/GfxEngine.hpp/.cpp` | PSRAM framebuffer, clear, flush |
| B4 | fillRect | Todo | | Status bar background |
| B5 | fillRoundedRect | Todo | | **Eye rendering** — milestone! |
| B6 | fillCircle, fillEllipse | Todo | | Pupils, dots, clouds |
| B7 | drawLine, drawDashedLine | Todo | | Rain, dividers, dotted leaders |
| B8 | Transform stack | Todo | | push/pop, translate, rotate, scale |
| B9 | Alpha blending | Todo | | Opacity fades |
| B10 | drawArc, drawBezier | Todo | | Arcs, heart, WiFi icon |
| B11 | fillPolygon | Todo | | Sad eye lids, check marks |
| B12 | drawText + font system | Todo | `lib/display/FontSystem.hpp` | 5 sizes (9/11/14/22/36px) |
| B13 | strokeCircle, strokeRoundedRect | Todo | | Sweep ring, progress bar |
| B14 | High-level: drawEye, drawEyes, fillHeart, fillStar | Todo | | Port emotion-core.jsx primitives |

**Milestone B5**: Sau bước này, variant `normal-steady` (hai mắt nhấp nháy) phải render đúng → first end-to-end validation.

---

## Phase C: System Integration

| # | Task | Status | File(s) | Ghi chú |
|---|------|--------|---------|---------|
| C1 | VariantRegistry | Done | `src/ui/VariantRegistry.hpp/.cpp` | Category/variant đăng ký |
| C2 | SceneManager | Done | `src/ui/SceneManager.hpp/.cpp` | Lifecycle, data injection, idle loop |
| C3 | SceneData struct | Done | `src/ui/SceneManager.hpp` | Live data cho scenes (in SceneManager.hpp) |
| C4 | StatusBar renderer | Todo | `src/ui/StatusBar.hpp/.cpp` | Fixed cyan, time + wifi + battery |
| C5 | DisplayManager refactor | Done | `src/system/DisplayManager.hpp/.cpp` | handleSyncData, handleOtaStatus, handleConnectivity |
| C6 | StateTypes rewrite | Done | `src/system/StateTypes.hpp` | ConnectionState(8), OtaState(8), EmotionState(16) |
| C7 | UartProtocol rewrite | Done | `lib/uart/UartProtocol.hpp` | SYNC_DATA, OTA_STATUS, LOG_ENTRY, DEVICE_CMD |
| C8 | UartBridge RX handlers | Done | `src/system/UartBridge.cpp` | Parse new msg types |

---

## Phase D: Port Emotions (37 categories, 168 variants)

Priority porting order:

| # | Category | Variants | Tone | Status | Ghi chú |
|---|----------|----------|------|--------|---------|
| D1 | normal | 16 | cyan | Todo | Idle state, most played |
| D2 | happy | 8 | warm | Todo | |
| D3 | sad | 6 | blue | Todo | Validates polygon lids |
| D4 | listening | 7 | cyan | Todo | Audio wave accessories |
| D5 | thinking | 7 | cyan | Todo | |
| D6 | loading | 5 | cyan | Todo | |
| D7 | greet | 4 | cyan | Todo | |
| D8 | angry | 4 | red | Todo | |
| D9 | love | 5 | rose | Todo | Heart shapes |
| D10 | excited | 5 | warm | Todo | |
| D11 | confused | 4 | orange | Todo | |
| D12 | sleepy | 4 | purple | Todo | |
| D13 | sleeping | 4 | purple | Todo | |
| D14 | error | 4 | red | Todo | |
| D15 | wink | 4 | warm | Todo | |
| D16 | shy | 4 | rose | Todo | |
| D17 | scared | 4 | white | Todo | |
| D18 | surprised | 4 | white | Todo | |
| D19 | crying | 4 | blue | Todo | |
| D20 | smug | 4 | warm | Todo | |
| D21 | proud | 4 | warm | Todo | |
| D22 | bored | 4 | cyan | Todo | |
| D23 | hungry | 4 | orange | Todo | |
| D24 | mischievous | 4 | purple | Todo | |
| D25 | focused | 4 | red | Todo | |
| D26 | charging | 4 | green | Todo | |
| D27 | dizzy | 4 | purple | Todo | |
| D28 | dead | 4 | red | Todo | |
| D29 | mute | 4 | cyan | Todo | |
| D30 | cool | 3 | cyan | Todo | |
| D31 | suspicious | 3 | purple | Todo | |
| D32 | curious | 3 | orange | Todo | |
| D33 | determined | 3 | red | Todo | |
| D34 | disgusted | 3 | green | Todo | |
| D35 | nervous | 3 | orange | Todo | |
| D36 | embarrassed | 3 | rose | Todo | |
| D37 | annoyed | 3 | orange | Todo | |

---

## Phase E: Port Scenes (21 categories, 59 variants)

| # | Category | Variants | Status | Ghi chú |
|---|----------|----------|--------|---------|
| E1 | boot | 4 | Todo | **DLunice-critical** — poweron, logo, checks, ready |
| E2 | network | 6 | Todo | **DLunice-critical** — wifi, ble, offline, server-error |
| E3 | weather | 5 | Todo | Data-driven (OpenWeather API) |
| E4 | clock | 3 | Todo | Data-driven (NTP) |
| E5 | music | 3 | Todo | |
| E6 | timer | 3 | Todo | |
| E7 | celebrate | 3 | Todo | Multi-color (confetti, firework) |
| E8 | fitness | 3 | Todo | |
| E9 | call | 3 | Todo | |
| E10 | message | 3 | Todo | |
| E11 | navigation | 3 | Todo | |
| E12 | cooking | 2 | Todo | |
| E13 | walking | 2 | Todo | |
| E14 | night | 2 | Todo | |
| E15 | camera | 2 | Todo | |
| E16 | gift | 2 | Todo | |
| E17 | coffee | 2 | Todo | |
| E18 | plant | 2 | Todo | |
| E19 | morning | 2 | Todo | |
| E20 | gaming | 2 | Todo | |
| E21 | calendar | 2 | Todo | |

Shared glyphs cần port trước E2: `WifiIcon`, `BTIcon`, `CloudIcon`, `MonoLabel`, `TypingDots`.

---

## Phase F: Data Integration

| # | Task | Status | Ghi chú |
|---|------|--------|---------|
| F1 | DataSyncManager (C5) | Done | Parse sync_data JSON, relay binary to S3 via UART |
| F2 | UART SYNC_DATA handler (S3) | Done | UartBridge dispatch + DisplayManager::handleSyncData |
| F3 | OTA_STATUS handler (S3) | Done | UartBridge dispatch + DisplayManager::handleOtaStatus |
| F4 | DEVICE_CMD handler (S3) | Done | UartBridge dispatch for server commands |
| F5 | Status bar live time | Todo | Hiển thị giờ realtime |
| F6 | Boot sequence async | Todo | Scene chạy song song subsystem init |
| F7 | Network scene auto-trigger | Done | ConnectionState → scene mapping in DisplayManager |
| F8 | DEVICE_CMD from server | Done | WS → WsMessageHandler → UART → S3 |

## Phase G: Firmware Refactor (WS-only + OTA + BLE v2)

> Refactor hoàn thành 2026-05-28. Xoá MQTT, chuyển sang WebSocket-only,
> thêm HTTP OTA, thống nhất state machine, cập nhật BLE provisioning.

| # | Task | Status | Ghi chú |
|---|------|--------|---------|
| G1 | C5 StateTypes rewrite | Done | ConnectionState(8), ConnectFailReason(11), OtaState(8), EmotionState(16), RetryPolicy |
| G2 | C5 ServerConfig + WsProtocol + WebSocketClient | Done | WS auth, JSON msg builders/parsers |
| G3 | C5 NetworkManager rewrite | Done | WS-only connection state machine, heartbeat |
| G4 | C5 WsMessageHandler | Done | Incoming WS message dispatcher |
| G5 | C5 OtaManager | Done | HTTP chunked download + SHA256 verify |
| G6 | C5 DataSyncManager | Done | Parse sync_data JSON, compact binary relay |
| G7 | C5 BluetoothService rewrite | Done | 12 chars, 3-level auth (PIN + HMAC-SHA256) |
| G8 | C5+S3 UartProtocol update | Done | SYNC_DATA, OTA_STATUS, LOG_ENTRY, DEVICE_CMD |
| G9 | C5+S3 UartBridge update | Done | New msg types, sendFrame DRY helper |
| G10 | C5 SpiBridge frame-aligned SPI | Done | Opus frame alignment, pending_header_ |
| G11 | S3 StateTypes rewrite | Done | ConnectionState, OtaState, EmotionState(16), SyncDataPacket |
| G12 | S3 DisplayManager update | Done | handleSyncData, handleOtaStatus, ConnectionState switch |
| G13 | S3 SceneManager SceneData update | Done | New fields: unix_time, temperature, humidity, aqi, city, battery |
| G14 | Delete MQTT (C5) | Done | git rm MqttClient, MQTTConfig, remove from CMake/platformio |
| G15 | C5 DeviceProfile rewrite | Done | No MQTT, device_token NVS, DeviceProfile class name |
| G16 | C5+S3 AppController update | Done | ConnectionState, no parseEmotionCode |
| G17 | Cleanup stale refs | Done | ConnectivityState→ConnectionState, CliConsole, SpiProtocol, Version.hpp |
| G18 | Build verified | Done | Both C5 + S3 compile successfully |

---

## Milestones

| Milestone | Mô tả | Target | Status |
|-----------|--------|--------|--------|
| M0 | Firmware refactor: WS-only + OTA + BLE v2 + state unification | — | Done |
| M1 | Docs hoàn thành | — | Done |
| M2 | GfxEngine renders `normal-steady` đúng | — | Todo |
| M3 | Boot sequence 4 scenes chạy trên device | — | Todo |
| M4 | Network scenes + ConnectionState wired | — | Todo |
| M5 | Status bar với live time (sync_data) | — | Todo |
| M6 | Weather scenes với live data (sync_data) | — | Todo |
| M7 | Tất cả 37 emotion categories ported | — | Todo |
| M8 | Tất cả 21 scene categories ported | — | Todo |
| M9 | Full system: 227 variants + real-time data | — | Todo |

---

## Notes

- Board hiện tại N4R8 (4MB flash) — cần chuyển sang N16R8 trước khi bắt đầu Phase B
- `convert_assets.py` giữ nguyên cho icons/logos — không dùng cho emotions/scenes
- AnimationPlayer (legacy 1-bit RLE) giữ hoạt động song song trong quá trình migration
- SPIFFS 12.9MB (trên N16R8) dùng cho: bitmap fonts, audio clips, config
