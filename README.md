# Luni — Desktop Companion Robot

**Luni** (Eye + Voice) is a desktop companion robot featuring an expressive screen-eye emotion system and voice assistant capabilities, built on ESP32 dual-chip architecture (ESP32-S3 + ESP32-C5).

> This project is the continuation and rebrand of the PTalk project. For historical reference, see the original repositories below.

## PrLunious Repositories

| Version | Repository | Description |
|---------|-----------|-------------|
| V1 | [trungnguyen278/PTalk](https://github.com/trungnguyen278/PTalk) | Original firmware (single ESP32) |
| V2 (base) | [trungnguyen278/PTalk-V2](https://github.com/trungnguyen278/PTalk-V2) | Dual-chip architecture (ESP32-S3 + ESP32-C5) |

## Project Structure

```
Luni/
├── esp32-s3/          # Display, audio, UI rendering (main MCU)
├── esp32-c5/          # Network, BLE, MQTT (communication MCU)
├── ui_design/         # Emotion & scene design source (JSX/SVG)
├── sim/               # DLunice & server simulators
└── docs/              # Architecture & pipeline documentation
```

## Status

Active development — new direction in progress.

## Author

**Trung Nguyen** — [@trungnguyen278](https://github.com/trungnguyen278)
