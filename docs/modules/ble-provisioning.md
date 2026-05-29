# Module: BLE Provisioning

C5 chạy NimBLE GATT (Service `0xFF01`) để App cấu hình robot lần đầu (WiFi, token, owner) và admin thao tác bảo trì. Code: `esp32-c5/src/system/BluetoothService`. Hướng dẫn chi tiết cho app dev: [esp32-c5/docs/BLE_APP_DEV.md](../../esp32-c5/docs/BLE_APP_DEV.md).

## 3 access level

| Level | Mở khoá bằng | Quyền |
|-------|--------------|-------|
| 0 — public | (mặc định) | Read DEVICE_INFO |
| 1 — user | ghi PIN 6 số (hiện trên màn hình robot) vào AUTH_UNLOCK | Read/Write provisioning, RESTART |
| 2 — admin | ghi admin token (HMAC, server cấp) vào ADMIN_AUTH | + DIAG_INFO, LOG_LEVEL, factory_reset, full_wipe, rollback_fw, … |

## GATT characteristics (Service 0xFF01)

| UUID | Tên | R/W | Level | Ghi chú |
|------|-----|-----|-------|---------|
| 0x0001 | SSID | R/W | 1 | WiFi SSID |
| 0x0002 | PASSWORD | W | 1 | WiFi password |
| 0x0003 | WS_URL | R/W | 1 (đổi domain: 2) | Server URL |
| 0x0005 | USER_ID | R/W | 1 | Owner user UUID |
| 0x0006 | DEVICE_INFO | R | 0 | `{ mac, model, fw_version, name }` |
| 0x0007 | DIAG_INFO | R | 2 | heap, uptime, reset_reason, rssi, partitions… |
| 0x0008 | DEV_TOKEN | W | 1 | device_token (128 hex, WS auth) |
| 0x0010 | AUTH_UNLOCK | W | 0→1 | PIN 6 số |
| 0x0011 | COMMAND | W/notify | 1/2 | byte lệnh (bảng dưới) |
| 0x0012 | ADMIN_AUTH | W | 1→2 | admin token (HMAC + ts) |
| 0x0013 | LOG_LEVEL | R/W | 2 | 10/20/30/40 |
| 0x0014 | ADMIN_SECRET | W | 1 | 32B, key verify admin token |

## COMMAND (ghi 1 byte vào 0x0011; device notify 0x00 OK / 0x01 FAIL / 0x02 UNAUTHORIZED)

| Byte | Lệnh | Level |
|------|------|-------|
| 0x01 | RESTART | 1 |
| 0x10 | FACTORY_RESET (xoá WiFi+token, giữ fw) | 2 |
| 0x11 | FULL_WIPE (xoá toàn bộ NVS) | 2 |
| 0x12 | ROLLBACK_FW | 2 |
| 0x13/0x14 | ENABLE/DISABLE_DEBUG | 2 |
| 0x15 | CLEAR_USERS (chỉ xoá user_id local) | 2 |
| 0x16 | ENTER_DFU | 2 |

Lệnh gây reboot: notify OK → delay 500ms → thực hiện → `esp_restart()`.

## Pairing flow (tóm tắt)

1. App scan service `0xFF01`, connect, read DEVICE_INFO (Level 0).
2. Ghi PIN → Level 1. Ghi SSID + PASSWORD.
3. App gọi server `POST /devices {mac,model,name}` → nhận `device_token` + `admin_secret`.
4. Ghi DEV_TOKEN (0x0008), USER_ID (0x0005), ADMIN_SECRET (0x0014), WS_URL (0x0003).
5. COMMIT + RESTART → robot connect WiFi → WS → auth bằng device_token.
6. App poll `GET /devices/{id}/status` đến khi `is_online=true`.

## Admin auth (Level 2)

```
admin_secret = HMAC-SHA256(mac, SERVER_SECRET_KEY)   # server cấp lúc register, app ghi 0x0014
token        = HMAC-SHA256(mac || timestamp, admin_secret)
```
Device verify token + `timestamp` trong ±5 phút (chống replay). MAC vĩnh viễn nên admin_secret ổn định.

## NVS reset behavior

| Key | factory_reset (0x10) | full_wipe (0x11) |
|-----|----------------------|------------------|
| wifi_ssid / wifi_password | ERASE | ERASE |
| device_token / admin_secret | ERASE | ERASE |
| ws_url / user_id / device_name | KEEP | ERASE |
| log_level | KEEP (→INFO) | ERASE |
| FW partitions | KEEP | KEEP |

`CLEAR_USERS` chỉ xoá `user_id` local; `devices.owner_id` / `device_shares` phía server không đổi (dùng web admin / API để gỡ quyền).
