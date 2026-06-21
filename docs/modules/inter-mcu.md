# Module: Inter-MCU Protocol (SPI + UART)

Hai MCU nối nhau bằng 2 bus tách biệt theo băng thông. **Nguyên tắc: SPI chỉ chạy audio, UART chạy control/status/sync.** Code: `lib/spi/SpiProtocol.hpp`, `lib/uart/UartProtocol.hpp`, `src/system/{SpiBridge,UartBridge}`.

## SPI — audio (256-byte full-duplex)

Frame: `[0xA5 magic][type:1][len:2 LE][payload:250][seq:1][crc8:1]`.

Chỉ 3 loại: `AUDIO_UPLINK` (S3→C5, Opus từ mic), `AUDIO_DOWNLINK` (C5→S3, Opus từ server), `EMPTY/HEARTBEAT` (kèm flow-control: số KB trống trong WS tx buffer).

### Frame-aligned Opus transfer (C5 → S3 downlink)

Vấn đề cũ: stream raw bytes qua SPI → khi S3 buffer đầy phải drop payload → mất alignment opus → cascade decode errors + resync 500ms.

Giải pháp: mỗi SPI payload chứa **trọn các opus frame**:
```
[frame_count:1] [len:2 LE][opus]  [len:2 LE][opus] ... [padding]
```
Mỗi opus frame có header 2-byte length. Frame không vừa transaction → giữ header lại (`pending_header_`) cho transaction sau. Kết quả: drop 1 payload = mất vài frame trọn vẹn → Opus PLC xử lý (gap ~60ms/frame), alignment luôn đúng, không cần resync.

Audio contract: **16 kHz mono, Opus VOIP, 60 ms frames, 24 kbps, constrained VBR** (`AudioConfig.hpp`) — mỗi frame encode ≤ 240B để `[len:2][opus]` lọt 1 payload SPI. Phía WS: uplink batch có tag `0xAA` đầu message; downlink là stream `[len:2 LE][opus]` không tag (xem `Luni_Cloud/server/app/utils/audio.py`).

## UART — control / status / sync (variable-length)

Frame: `[0x55 start][type:1][len:1][payload:0-250][crc8:1]`.

| MsgType | Hướng | Nội dung |
|---------|-------|----------|
| STATUS_UPDATE (0x01) | C5→S3 | snapshot interaction + connection + system + emotion (+ fail_reason) |
| CONTROL_CMD (0x02) | S3→C5 | START, END, SET_VOLUME, REBOOT, SET_BRIGHTNESS, WIFI_CONFIG, WS_CONFIG |
| SYNC_DATA (0x03) | C5→S3 | weather/time/lunar/city (binary packed, ~40B) |
| OTA_STATUS (0x04) | C5→S3 | OTA state + progress % |
| LOG_ENTRY (0x05) | S3→C5 | log từ S3 → forward server |
| DEVICE_CMD (0x06) | C5→S3 | command từ server/app: SET_EMOTION, SET_SCENE, TTS_PLAY, AUDIO_STOP, CONFIG_UPDATE, INTERACTION_MSG |

> MQTT đã bỏ — không còn `MQTT_CONFIG`. Mọi command từ server map qua `DEVICE_CMD`.

### SYNC_DATA payload (UART, C5 → S3)

```
[temp:1][feels:1][humidity:1][condition:1 enum][aqi:1]
[unix_time:4 LE][utc_offset:1][lunar_day:1][lunar_month:1]
[city: null-terminated]
```
S3 `DisplayManager` nhận → cập nhật `SceneManager` LiveData → re-render scene (weather/clock/calendar). Xem [SCENE_DATA_SPEC.md](../SCENE_DATA_SPEC.md).
