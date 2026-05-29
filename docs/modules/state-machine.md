# Module: State Machine

C5 sở hữu Connection + OTA state, relay sang S3 để hiển thị. Interaction + Emotion state dùng chung. Định nghĩa: `esp32-c5/src/system/StateTypes.hpp`, quản lý bởi `StateManager` (thread-safe pub-sub).

## Connection state (8) — C5

```
OFFLINE → WIFI_CONNECTING → WIFI_CONNECTED → WS_CONNECTING
        → WS_AUTHENTICATING → ONLINE → (lost) → RECONNECTING
        ↘ BLE_PROVISIONING (không có credentials / bị reject)
```

| State | Mô tả |
|-------|-------|
| OFFLINE | Chưa có WiFi |
| WIFI_CONNECTING | Đang scan/associate |
| WIFI_CONNECTED | Có IP |
| WS_CONNECTING | DNS + TCP + WS handshake |
| WS_AUTHENTICATING | Gửi `auth` (device_token + MAC), chờ kết quả ≤ 5s |
| ONLINE | Đã auth, gửi device_info + heartbeat |
| RECONNECTING | Mất kết nối, auto-retry theo backoff |
| BLE_PROVISIONING | Chế độ cấu hình BLE |

## ConnectFailReason → retry strategy

Mỗi transition khi fail set một `ConnectFailReason` → S3 hiển thị đúng thông báo, C5 quyết định retry.

| Reason | Retry | Backoff | Fallback |
|--------|-------|---------|----------|
| WIFI_NOT_FOUND / WIFI_TIMEOUT | 3x | 5s | BLE_PROVISIONING |
| WIFI_AUTH_FAIL | 0 | — | BLE_PROVISIONING |
| DHCP_FAIL | 3x | 5s | BLE_PROVISIONING |
| DNS_FAIL / INTERNET_CHECK_FAIL | 5x | 10s | BLE_PROVISIONING (nghi sai WiFi) |
| SERVER_UNREACHABLE | 10x | exp 1→30s | OFFLINE (chờ) |
| WS_HANDSHAKE_FAIL | 5x | exp 2→30s | OFFLINE |
| TLS_ERROR | 3x | 5s | OFFLINE |
| AUTH_REJECTED (WS close 4001) | 0 | — | BLE_PROVISIONING (re-pair) |
| AUTH_TIMEOUT | 3x | exp 2→10s | RECONNECTING |

**3 nhóm lỗi chính:** không có WiFi → BLE; có WiFi nhưng không Internet → BLE (sai mạng?); có Internet nhưng server down → retry rồi OFFLINE.

## Interaction state (6) — dùng chung S3+C5

```
IDLE → TRIGGERED → LISTENING → PROCESSING → SPEAKING → IDLE
                       ↘ CANCELLING ↗
```
Trigger: button / wake-word / app. LISTENING stream audio uplink; PROCESSING = server STT+LLM; SPEAKING = phát TTS downlink.

## OTA state (8) — C5 manage, S3 display

```
IDLE → CHECKING → AVAILABLE → DOWNLOADING → VERIFYING → FLASHING → REBOOTING
                                  ↘ FAILED (→ IDLE / retry CHECKING)
```

## Emotion state (16) — S3 manage, C5 relay từ server

NEUTRAL, HAPPY, SAD, ANGRY, CONFUSED, EXCITED, CALM, THINKING, DISGUSTED, NERVOUS, EMBARRASSED, CURIOUS, ANNOYED, COOL, SUSPICIOUS, DETERMINED.

## StateEvent

`StateManager` phát `StateEvent { type (CONNECTION|INTERACTION|OTA|SYSTEM|POWER|EMOTION), old, new, fail_reason, timestamp }`. C5 relay event sang S3 qua UART `STATUS_UPDATE`.
