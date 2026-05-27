# Luni Robot — Firmware

**Luni** is a desktop companion robot featuring an expressive screen-eye emotion system and voice assistant capabilities, built on ESP32 dual-chip architecture (ESP32-S3 + ESP32-C5).

> Rebrand from PTalk project. See original repos below.

## Architecture

```
Luni_Robot/
├── esp32-s3/          # Display, audio, UI rendering (main MCU)
├── esp32-c5/          # Network, BLE, WebSocket (communication MCU)
├── ui_design/         # Emotion & scene design source (JSX/SVG)
├── sim/               # Device & server simulators
└── docs/              # Architecture & plan documentation
```

**Dual-chip:**
- **ESP32-S3** — 240MHz, TFT display (GC9A01), I2S audio (mic + speaker), Opus codec, procedural UI rendering
- **ESP32-C5** — WiFi 6, BLE 5, WebSocket client, OTA via HTTP, SPI slave (audio bridge)
- **SPI** — 256-byte full-duplex frames (audio only)
- **UART** — variable-length frames (control, status, sync_data)

## Documentation

- [System Architecture](docs/plan/SYSTEM_ARCHITECTURE.md) — tổng quan toàn hệ thống
- [Robot Plan](docs/plan/PLAN_ROBOT.md) — chi tiết firmware refactoring, state machine, protocols

## Previous Versions

| Version | Repository | Description |
|---------|-----------|-------------|
| V1 | [trungnguyen278/PTalk](https://github.com/trungnguyen278/PTalk) | Original firmware (single ESP32) |
| V2 (base) | [trungnguyen278/PTalk-V2](https://github.com/trungnguyen278/PTalk-V2) | Dual-chip architecture (ESP32-S3 + ESP32-C5) |

## Related Repos

| Repo | Mô tả |
|------|--------|
| [Luni_Cloud](https://github.com/trungnguyen278/Luni_Cloud) | FastAPI server + Next.js web admin (Docker Compose) |
| [Luni_App](https://github.com/trungnguyen278/Luni_App) | Flutter mobile app (iOS/Android) — BLE pairing, device control |

## Author

**Trung Nguyen** — [@trungnguyen278](https://github.com/trungnguyen278)
