# Luni V2 — ESP32-C5 Firmware

Firmware cho MCU mang (network MCU) trong kien truc dual-MCU cua Luni Robot.
ESP32-C5 dam nhan WiFi, WebSocket, BLE provisioning, va giao tiep voi ESP32-S3 qua SPI + UART.

## Kien Truc Dual-MCU

```
ESP32-C5 (Network MCU)              ESP32-S3 (Media MCU)
+---------------------------+       +---------------------------+
| WiFi 802.11b/g/n/ax      |       | Display ST7789 240x240    |
| WebSocket Client (WSS)   | SPI   | Audio I2S (Mic + Speaker) |
| BLE Provisioning (NimBLE) |<----->| Power Management (ADC)    |
| OTA Manager (HTTP)        | UART  | Touch/Button Input        |
| DataSyncManager           |<----->| Scene/Emotion Rendering   |
+---------------------------+       +---------------------------+
```

- **SPI**: Full-duplex 256-byte transactions — audio Opus frames (frame-aligned)
- **UART**: Variable-length frames — control/status/sync messages

## Tinh Nang

- **WebSocket**: Persistent WSS connection voi device token authentication
- **BLE Provisioning**: 3-level access control (PIN + HMAC-SHA256 admin auth), 12 GATT characteristics
- **OTA Update**: HTTP chunked download voi SHA256 verification
- **Data Sync**: Nhan time/weather/lunar/location tu server, relay sang S3
- **State Machine**: 8 connection states voi fail-reason-based retry + BLE fallback
- **CLI Console**: UART0 debug console (status, config, set wifi/ws/token/name)

## Yeu Cau

- **Platform**: ESP32-C5 (RISC-V)
- **Framework**: ESP-IDF 5.5 + PlatformIO
- **C++ Standard**: C++17
- **Flash**: 4MB

## Cau Truc Du An

```
esp32-c5/
+-- src/
|   +-- main.cpp                         # Entry point
|   +-- AppController.cpp/hpp            # Central orchestrator, event queue
|   +-- Version.hpp                      # APP_VERSION, DLuniCE_MODEL
|   +-- config/
|   |   +-- DeviceProfile.cpp/hpp        # Dependency injection, module wiring
|   |   +-- PinConfig.hpp                # GPIO assignments (SPI, UART, NVS reset)
|   |   +-- ServerConfig.hpp             # WS URL, OTA URL defaults
|   +-- protocol/
|   |   +-- WsProtocol.cpp/hpp           # WS message types, JSON builders/parsers
|   |   +-- WsMessageHandler.cpp/hpp     # Incoming WS message dispatcher
|   +-- system/
|   |   +-- StateManager.cpp/hpp         # Thread-safe state hub (pub-sub)
|   |   +-- StateTypes.hpp               # ConnectionState(8), InteractionState(6),
|   |   |                                # OtaState(8), EmotionState(16), etc.
|   |   +-- NetworkManager.cpp/hpp       # WiFi + WS lifecycle, heartbeat, state sync
|   |   +-- BluetoothService.cpp/hpp     # NimBLE GATT, 3-level auth, 12 characteristics
|   |   +-- DataSyncManager.cpp/hpp      # Parse sync_data JSON, relay binary to S3
|   |   +-- OtaManager.cpp/hpp           # HTTP OTA download + verify
|   |   +-- LogManager.cpp/hpp           # Forward logs to server via WS
|   |   +-- SpiBridge.cpp/hpp            # SPI slave, frame-aligned Opus transfer
|   |   +-- UartBridge.cpp/hpp           # UART TX/RX, 6 message types
|   |   +-- CliConsole.cpp/hpp           # UART0 debug CLI
+-- lib/
|   +-- network/
|   |   +-- WebSocketClient.cpp/hpp      # ESP websocket client wrapper
|   |   +-- HttpClient.cpp/hpp           # HTTP client for OTA
|   |   +-- WifiService.cpp/hpp          # WiFi STA management
|   +-- uart/
|   |   +-- UartProtocol.hpp             # Frame format, MsgType enum, StatusPayload
|   +-- spi/
|   |   +-- SpiProtocol.hpp              # 256-byte frame format, CRC8
+-- docs/
|   +-- BLE_APP_DEV.md                   # Mobile app BLE provisioning guide
+-- platformio.ini
+-- sdkconfig.esp32-c5
```

## State Management

### Connection State (C5 owns, relays to S3)
| State | Description |
|-------|-------------|
| OFFLINE | No WiFi |
| WIFI_CONNECTING | Scanning/connecting |
| WIFI_CONNECTED | Got IP |
| WS_CONNECTING | TCP + WS handshake |
| WS_AUTHENTICATING | Sending device_token |
| ONLINE | Fully connected |
| RECONNECTING | Auto-retry with backoff |
| BLE_PROVISIONING | BLE config mode |

### Retry Strategy
Dua tren `ConnectFailReason`:
- **WIFI_AUTH_FAIL / AUTH_REJECTED**: Khong retry, fallback BLE provisioning
- **SERVER_UNREACHABLE / WS_HANDSHAKE_FAIL**: Exponential backoff (1s-30s, max 10 retries)
- **WIFI_NOT_FOUND / WIFI_TIMEOUT**: 3 retries, fallback BLE
- **DNS_FAIL / INTERNET_CHECK_FAIL**: 5 retries, fallback BLE

### Interaction State (shared C5 + S3)
IDLE -> TRIGGERED -> LISTENING -> PROCESSING -> SPEAKING -> IDLE

### Emotion State (16 values)
NEUTRAL, HAPPY, SAD, ANGRY, CONFUSED, EXCITED, CALM, THINKING,
DISGUSTED, NERVOUS, EMBARRASSED, CURIOUS, ANNOYED, COOL, SUSPICIOUS, DETERMINED

## Communication Protocols

### UART Protocol (C5 <-> S3)
Frame: `[0x55 start][type:1][len:1][payload:0-250][crc8:1]`

| MsgType | Direction | Description |
|---------|-----------|-------------|
| STATUS_UPDATE (0x01) | C5->S3 | interaction + connection + system + emotion |
| CONTROL_CMD (0x02) | S3->C5 | START, END, SET_VOLUME, REBOOT, etc. |
| SYNC_DATA (0x03) | C5->S3 | Weather, time, lunar, city (binary packed) |
| OTA_STATUS (0x04) | C5->S3 | OTA state + progress percent |
| LOG_ENTRY (0x05) | S3->C5 | S3 logs forwarded to server |
| DEVICE_CMD (0x06) | C5->S3 | Server commands (emotion, scene, TTS, etc.) |

### SPI Protocol (C5 <-> S3)
Fixed 256-byte full-duplex: `[0xA5 magic][type:1][len:2 LE][payload:250][seq:1][crc8:1]`

**Frame-aligned Opus transfer** (C5->S3 downlink):
```
[frame_count:1][len_hi:1][len_lo:1][opus_data...]...[padding]
```
Moi Opus frame co 2-byte length header. Neu frame khong vua trong transaction,
header duoc giu lai (`pending_header_`) cho transaction tiep theo.

### WebSocket Protocol (C5 <-> Server)
- **Text frames**: JSON messages (auth, device_info, state_update, heartbeat, etc.)
- **Binary frames**: Opus audio (5-byte header: direction + sequence + length)
- **Authentication**: Device token gui ngay sau WS handshake

## Build & Flash

```bash
# Build
pio run

# Upload + Monitor
pio run -t upload && pio run -t monitor

# Monitor only
pio run -t monitor
```

## CLI Commands

Ket noi UART0 (115200 baud):

```
help                  Show commands
status                Show state + heap
config                Show NVS config
set wifi <ssid> [pw]  Set WiFi credentials
set ws <url>          Set WebSocket URL
set token <tok>       Set device token
set name <name>       Set device name
erase_nvs             Erase all NVS & restart
restart               Restart device
```

## Tai Lieu

- [BLE_APP_DEV.md](docs/BLE_APP_DEV.md) — Mobile app BLE provisioning guide
- [docs/architecture.md](../docs/architecture.md) — Dual-MCU architecture
- [docs/modules/network.md](../docs/modules/network.md) — WiFi / WebSocket / OTA
- [docs/modules/state-machine.md](../docs/modules/state-machine.md) — Connection / interaction / OTA states
- [docs/modules/inter-mcu.md](../docs/modules/inter-mcu.md) — SPI + UART protocols

## License

[TBD]

**Version**: 2.0.0
**Model**: Luni-C5
**Status**: Development
