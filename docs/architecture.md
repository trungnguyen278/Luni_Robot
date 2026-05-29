# Architecture — Dual-MCU

Luni Robot dùng 2 MCU chia rõ vai trò: **ESP32-C5** lo mạng, **ESP32-S3** lo media/UI. Hai MCU nối nhau qua SPI (audio) + UART (control/status). Giao thức inter-MCU đã ổn định.

```
ESP32-C5 (Network MCU)                  ESP32-S3 (Media MCU)
┌────────────────────────┐             ┌────────────────────────┐
│ WiFi 802.11ax          │     SPI     │ Display ST7789 (RGB565) │
│ WebSocket client (WSS) │◄═══════════►│ Audio I2S (mic+speaker) │
│ BLE provisioning(NimBLE)│  256B audio │ Opus codec              │
│ OTA HTTP               │     UART    │ Scene / emotion render  │
│ DataSyncManager        │◄═══════════►│ Touch / button input    │
│ SPI + UART master/slave│ control/sync│ Power management (ADC)  │
└────────────────────────┘             └────────────────────────┘
        │ WSS + HTTP OTA
        ▼
   Luni Cloud (FastAPI) ── lunirobot.io.vn
```

## Vai trò

| | ESP32-C5 (Network) | ESP32-S3 (Media) |
|--|--------------------|------------------|
| CPU | RISC-V | 240MHz Xtensa |
| Lo | WiFi 6, BLE 5, WebSocket, OTA, server comms, data sync | Display, audio I/O, Opus, UI rendering, emotion, power |
| Auth | giữ `device_token`, gửi auth khi WS connect | — |
| Flash | 4MB | 16MB (N16R8) + 8MB PSRAM |

## Kênh giao tiếp

| Kênh | Loại | Nội dung |
|------|------|----------|
| C5 ↔ Cloud | WSS persistent | commands, state, audio (Opus binary), logs, sync_data |
| C5 ↔ Cloud | HTTP | OTA firmware download |
| C5 ↔ App | BLE | provisioning 1 lần (WiFi, token, admin) |
| S3 ↔ C5 | SPI 256B full-duplex | audio Opus frames (frame-aligned) |
| S3 ↔ C5 | UART variable | control, status, sync_data, OTA status, log relay, device cmd |

Chi tiết: [modules/network.md](modules/network.md), [modules/inter-mcu.md](modules/inter-mcu.md).

## Cấu trúc thư mục

```
Luni_Robot/
├── esp32-c5/      # Network MCU firmware (xem esp32-c5/README.md)
├── esp32-s3/      # Media MCU firmware (display, audio, UI)
├── ui_design/     # Nguồn thiết kế emotion/scene (JSX/SVG) → C++
├── sim/           # Device & server simulator
└── docs/          # Tài liệu này
```

## Build & flash

```bash
# Mỗi MCU build riêng bằng PlatformIO
cd esp32-c5 && pio run -t upload && pio run -t monitor
cd esp32-s3 && pio run -t upload && pio run -t monitor
```

Yêu cầu: ESP-IDF 5.5 + PlatformIO, C++17.

> Status: Development. Phần UI rendering trên S3 đang được port từ thiết kế — xem [PROGRESS.md](PROGRESS.md).
