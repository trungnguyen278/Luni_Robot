# Luni Robot — Documentation

Firmware dual-MCU (ESP32-S3 + ESP32-C5) cho robot companion Luni. Tài liệu chia theo **module**.

## Architecture & firmware modules

| Doc | Nội dung |
|-----|----------|
| [architecture.md](architecture.md) | Dual-MCU overview, vai trò S3 vs C5, kênh giao tiếp, build/flash |
| [modules/state-machine.md](modules/state-machine.md) | Connection / Interaction / OTA state machine + retry theo fail-reason |
| [modules/network.md](modules/network.md) | WiFi, WebSocket client, auth, OTA HTTP, reconnect |
| [modules/inter-mcu.md](modules/inter-mcu.md) | Giao thức SPI (audio) + UART (control/status/sync), frame-aligned Opus |
| [modules/ble-provisioning.md](modules/ble-provisioning.md) | BLE pairing: 3 access level, GATT characteristics, admin auth, NVS reset |

## UI / rendering (ESP32-S3)

| Doc | Nội dung |
|-----|----------|
| [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) | GfxEngine, framebuffer, performance |
| [SCENE_DATA_SPEC.md](SCENE_DATA_SPEC.md) | Scene registry, data flow, LiveData |
| [UI_DESIGN_PIPELINE.md](UI_DESIGN_PIPELINE.md) | Pipeline JSX/SVG → procedural C++ |
| [PROGRESS.md](PROGRESS.md) | Tracker chuyển 227 UI variants sang C++ (đang làm) |

## Per-MCU

- [esp32-c5/README.md](../esp32-c5/README.md) — network MCU (WiFi/WS/BLE/OTA)
- [esp32-c5/docs/BLE_APP_DEV.md](../esp32-c5/docs/BLE_APP_DEV.md) — chi tiết BLE cho app dev

## Related repos

| Repo | Mô tả |
|------|--------|
| [Luni_Cloud](https://github.com/trungnguyen278/Luni_Cloud) | FastAPI server + web admin |
| [Luni_App](https://github.com/trungnguyen278/Luni_App) | Flutter app (BLE pairing, control) |
