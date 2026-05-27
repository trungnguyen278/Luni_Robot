# Luni MQTT Communication Specification (MEO SDK Compatible)

Tài liệu này đặc tả cấu trúc bản tin và quy trình giao tiếp giữa **Luni Server** và **Luni DLunice** (ESP32) thông qua giao thức MQTT, tương thích với **MEO SDK**.

## 1. Tổng quan kiến trúc

Luni hỗ trợ 2 chế độ giao tiếp MQTT:
- **MEO SDK Mode**: Tương thích với MEO Arduino SDK (feature invoke/event model)
- **Legacy Mode**: Tương thích ngược với giao thức Luni cũ

---

## 2. MEO SDK Topic Patterns

### 2.1 Namespace và Topic Structure

| Pattern | Hướng | Mô tả |
|---------|-------|-------|
| `meo/{userId}/{dLuniceId}/feature` | Server → DLunice | Cloud-compatible feature invoke |
| `meo/{dLuniceId}/feature/{featureName}/invoke` | Server → DLunice | Legacy feature invoke |
| `meo/{userId}/{dLuniceId}/event/{eventName}` | DLunice → Server | DLunice events |
| `meo/{userId}/{dLuniceId}/event/feature_response` | DLunice → Server | Feature invoke response |

### 2.2 DLunice Identity

- **dLuniceId**: MAC address hex string (12 ký tự viết hoa, ví dụ: `D4E9F4C13B1C`)
- **userId**: Namespace cho multi-tenant (default: `"default"`)
- **tx_key**: MQTT password (được provision qua BLE)

### 2.3 Topic Examples

```
# Cloud-compatible invoke (userId = "user42", dLuniceId = "D4E9F4C13B1C")
meo/user42/D4E9F4C13B1C/feature

# Legacy invoke (dLuniceId only)
meo/D4E9F4C13B1C/feature/set_volume/invoke

# Event publishing
meo/user42/D4E9F4C13B1C/event/status
meo/user42/D4E9F4C13B1C/event/feature_response
meo/user42/D4E9F4C13B1C/event/battery
```

---

## 3. Feature Invoke Model

### 3.1 Cloud-Compatible Invoke (Preferred)

**Topic**: `meo/{userId}/{dLuniceId}/feature`

**Payload**:
```json
{
  "feature": "set_volume",
  "params": {
    "volume": 75
  },
  "invoke_id": "abc123"
}
```

Hoặc sử dụng `feature_name` thay cho `feature`:
```json
{
  "feature_name": "set_brightness",
  "params": {
    "brightness": 80
  }
}
```

### 3.2 Legacy Invoke

**Topic**: `meo/{dLuniceId}/feature/{featureName}/invoke`

**Payload**:
```json
{
  "params": {
    "volume": 75
  }
}
```

### 3.3 Feature Response

**Topic**: `meo/{userId}/{dLuniceId}/event/feature_response`

**Payload**:
```json
{
  "feature_name": "set_volume",
  "dLunice_id": "D4E9F4C13B1C",
  "success": true,
  "message": "Volume set to 75",
  "invoke_id": "abc123",
  "data": {
    "volume": "75"
  }
}
```

---

## 4. Built-in Features (Luni)

### 4.1 DLunice Configuration Features

| Feature Name | Params | Description |
|--------------|--------|-------------|
| `set_volume` | `{"volume": 0-100}` | Set speaker volume |
| `set_brightness` | `{"brightness": 0-100}` | Set display brightness |
| `set_dLunice_name` | `{"dLunice_name": "string"}` | Set dLunice display name |
| `set_wifi` | N/A | **Not supported** - Use BLE provisioning |

### 4.2 DLunice Control Features

| Feature Name | Params | Description |
|--------------|--------|-------------|
| `request_status` | (none) | Request full dLunice status |
| `reboot` | (none) | Reboot dLunice |
| `request_ble_config` | (none) | Enter BLE config mode |
| `request_ota` | `{"size": uint32, "sha256": "...", "chunk_size": int, "total_chunks": int}` | Initiate OTA update |

### 4.3 Luni-Specific Features

| Feature Name | Params | Description |
|--------------|--------|-------------|
| `set_emotion` | `{"code": "01"}` | Set display emotion (2-char code) |
| `play_tts` | `{"text": "...", "voice": "..."}` | Play TTS audio |
| `stop_audio` | (none) | Stop current audio playback |

---

## 5. DLunice Events

### 5.1 Event Structure

**Topic**: `meo/{userId}/{dLuniceId}/event/{eventName}`

**Payload**:
```json
{
  "event": "status",
  "dLunice_id": "D4E9F4C13B1C",
  "key": "value",
  ...
}
```

### 5.2 Built-in Events

| Event Name | Data Fields | Description |
|------------|-------------|-------------|
| `status` | `connectivity`, `firmware_version`, `battery_percent`, ... | DLunice status update |
| `feature_response` | `feature_name`, `success`, `message`, `data` | Response to feature invoke |
| `battery` | `percent`, `charging` | Battery status change |
| `connectivity` | `state`, `rssi` | Connectivity change |
| `ota_progress` | `percent`, `bytes_received` | OTA download progress |
| `ota_complete` | `success`, `message` | OTA completion status |
| `error` | `code`, `message` | Error notification |

---

## 6. Legacy Luni Topics (Backward Compatibility)

Các topic cũ vẫn được hỗ trợ để tương thích ngược:

| Topic | Hướng | Mô tả |
|-------|-------|-------|
| `dLunices/{MAC}/cmd` | Server → DLunice | JSON config commands |
| `dLunices/{MAC}/status` | DLunice → Server | DLunice status (retained) |
| `dLunices/{MAC}/ota_data` | Server → DLunice | OTA binary chunks |
| `dLunices/{MAC}/ota_ack` | DLunice → Server | OTA acknowledgments |

### 6.1 Legacy Command Format

```json
{
  "cmd": "set_volume",
  "volume": 75
}
```

---

## 7. BLE Provisioning Characteristics

### 7.1 Service UUID

**Service**: `0xFF01`

### 7.2 Characteristics

| UUID | Name | Access | Description |
|------|------|--------|-------------|
| `0xFF02` | DLuniCE_NAME | RW | DLunice display name |
| `0xFF03` | VOLUME | RW | Volume level (0-100) |
| `0xFF04` | BRIGHTNESS | RW | Brightness level (0-100) |
| `0xFF05` | WIFI_SSID | W | WiFi SSID |
| `0xFF06` | WIFI_PASS | W | WiFi password |
| `0xFF07` | APP_VERSION | R | Firmware version |
| `0xFF08` | BUILD_INFO | R | Build info string |
| `0xFF09` | SAVE_CMD | W | Save config (write 0x01) |
| `0xFF0A` | DLuniCE_ID | R | DLunice MAC string |
| `0xFF0B` | WIFI_LIST | R | Scanned WiFi list (streaming) |
| `0xFF0C` | WS_URL | RW | WebSocket URL (auth required) |
| `0xFF0D` | MQTT_URL | RW | MQTT broker URL (auth required) |
| `0xFF0E` | USER_ID | RW | MEO user ID |
| `0xFF0F` | TX_KEY | W | MEO tx_key / MQTT password |
| `0xFF10` | PRODUCT_ID | R | Cloud product ID |
| `0xFF11` | DEV_MODEL | R | DLunice model |
| `0xFF12` | DEV_MANUF | R | DLunice manufacturer |
| `0xFF13` | MAC_ADDR | R | Raw MAC address (6 bytes) |

### 7.3 Authentication

Các characteristics nhạy cảm (WS_URL, MQTT_URL, TX_KEY) yêu cầu unlock trước khi write:
1. Write auth token `"Luni_OK"` vào characteristic
2. Sau khi unlock, có thể read/write giá trị thực

---

## 8. OTA Protocol

### 8.1 Initiate OTA

**Feature invoke**:
```json
{
  "feature": "request_ota",
  "params": {
    "size": 1234567,
    "sha256": "abc123...",
    "chunk_size": 2048,
    "total_chunks": 603
  }
}
```

### 8.2 Binary Chunk Format

**Topic**: `dLunices/{MAC}/ota_data` hoặc `meo/{userId}/{dLuniceId}/ota_data`

**Binary Header (12 bytes, Little Endian)**:
| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Sequence number (0, 1, 2...) |
| 4 | 4 | Chunk data size |
| 8 | 4 | CRC32 of chunk data |
| 12+ | N | Chunk data |

### 8.3 OTA Acknowledgment

**Topic**: `dLunices/{MAC}/ota_ack`

**ACK**:
```json
{"ota_ack": 0}
```

**NACK**:
```json
{"ota_nack": 5, "expected_seq": 4}
```

---

## 9. Configuration Summary

| Parameter | Value |
|-----------|-------|
| MQTT Buffer Size | 4096 bytes |
| Keep Alive | 60 seconds |
| Command QoS | 1 |
| OTA Data QoS | 1 |
| Status QoS | 1 (retained) |
| Max JSON Length | 1024 bytes |
| Max Binary Length | 4000 bytes |
| OTA Chunk Size | 2048 bytes (recommended) |

---

## 10. Migration Guide

### From Legacy Luni to MEO SDK

1. **Topics**: Change `dLunices/{MAC}/cmd` → `meo/{userId}/{dLuniceId}/feature`
2. **Commands**: Wrap in feature invoke format
3. **Responses**: Parse from `feature_response` event topic

### Example Migration

**Old (Legacy)**:
```
Topic: dLunices/D4E9F4C13B1C/cmd
Payload: {"cmd": "set_volume", "volume": 75}
```

**New (MEO SDK)**:
```
Topic: meo/default/D4E9F4C13B1C/feature
Payload: {"feature": "set_volume", "params": {"volume": 75}}
```

---

*Document updated: 2026-02-04*
*MEO SDK Version: 3.x Compatible*
