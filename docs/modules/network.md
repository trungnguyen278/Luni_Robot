# Module: Network (WiFi · WebSocket · OTA)

ESP32-C5 lo toàn bộ kết nối. Code: `lib/network/{WifiService,WebSocketClient,HttpClient}`, `src/system/{NetworkManager,OtaManager,DataSyncManager}`, `src/protocol/{WsProtocol,WsMessageHandler}`.

## NetworkManager

Chạy connection state machine ([state-machine.md](state-machine.md)) trên task riêng: WiFi → WS connect → auth → ONLINE, kèm heartbeat timer (30s) và auto-reconnect. Quản lý `WifiService`, `WebSocketClient`, `OtaManager`, `DataSyncManager`, `LogManager`.

## WebSocket client

Server URL mặc định trong `ServerConfig.hpp` / `DeviceProfile.cpp`:
```
ws://lunirobot.io.vn/ws/device     # ghi đè được qua BLE / CLI, lưu NVS "ws_url"
```

### Auth handshake (ngay sau WS open)

```
device → { "type":"auth", "payload":{ "device_token","mac","fw_version","model" } }
server → { "type":"auth_result", "payload":{ "status":"ok" } }   # timeout 5s
```
- `mac` = BLE MAC (`ESP_MAC_BT`), định danh vĩnh viễn (sống sót NVS reset).
- `device_token` (128 hex) provisioned qua BLE, lưu NVS `"device_token"`.
- Fail/timeout → server close **4001** → C5 chuyển BLE_PROVISIONING (re-pair).

### Frames

- **Text**: JSON message (`WsProtocol.hpp` map `type` string ↔ `MsgType` enum). Device gửi: `device_info`, `state_update`, `heartbeat`, `log`, `ota_progress`, `error`. Nhận: `set_volume/brightness/emotion/scene`, `reboot`, `ota_available`, `sync_data`, `tts_play`, `audio_stop`, `config_update`, `interaction_msg`, `ack`.
- **Binary**: Opus audio 16 kHz mono VOIP 60 ms. Uplink: mỗi WS message = `[0xAA][len:2 LE][opus]...` (batch tag 0xAA). Downlink: stream `[len:2 LE][opus]...` không tag. Camera: 0xAC (nguyên ảnh) / 0xAD (chunk).

`WsMessageHandler` dispatch message đến, relay command sang S3 qua UART `DEVICE_CMD`.

## OTA (HTTP)

`OtaManager` — download chunked qua `HttpClient`, verify SHA256, ghi partition, reboot.

```
IDLE → CHECKING → AVAILABLE → DOWNLOADING → VERIFYING → FLASHING → REBOOTING
```
Trigger: server gửi `ota_available { version, url, sha256, size }`. Tiến trình báo về server bằng `ota_progress { percent, phase }`. Lỗi → `FAILED`, hỗ trợ rollback partition.

## DataSyncManager

Nhận `sync_data` (time/weather/lunar/location) từ server, cache, đóng gói binary và relay sang S3 qua UART `SYNC_DATA` để render scene. Xem [inter-mcu.md](inter-mcu.md).

## CLI (UART0, 115200)

`status`, `config`, `set wifi <ssid> [pw]`, `set ws <url>`, `set token <tok>`, `set name <name>`, `erase_nvs`, `restart`.
