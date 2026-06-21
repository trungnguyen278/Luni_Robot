# Module: State Machine

C5 sở hữu Connection + OTA state và **voice turn** (Interaction); S3 mirror qua UART `STATUS_UPDATE` và sở hữu Power + render Emotion. Định nghĩa: `esp32-c5/src/system/StateTypes.hpp` và `esp32-s3/src/system/StateTypes.hpp` (**hai bản copy phải giữ đồng bộ** — giá trị enum đi qua UART dưới dạng byte thô), quản lý bởi `StateManager` (thread-safe pub-sub).

## Connection state (10) — C5

```
OFFLINE → WIFI_CONNECTING → WIFI_CONNECTED → WS_CONNECTING
        → WS_AUTHENTICATING → ONLINE → (lost) → RECONNECTING
        ↘ BLE_PROVISIONING ⇄ BLE_CONNECTED (không có credentials / bị reject)
WS_ERROR: WiFi OK nhưng server unreachable
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
| WS_ERROR | WiFi OK, server không tới được |
| BLE_CONNECTED | App đã nối BLE trong lúc provisioning |

Transition được validate cứng trong `isValidConnectionTransition`; `NetworkManager::transition()` log **ERROR** nếu bị từ chối (nghĩa là bảng và flow lệch nhau — bug). Race "auth xong trong poll đầu tiên" được xử lý: luôn announce `WS_AUTHENTICATING` trước khi vào `ONLINE`.

**Reconnect nhanh:** rớt sạch từ ONLINE (reason = NONE) → lần retry đầu chỉ chờ **500ms** (blip vài trăm ms không phải trả 5s backoff); từ lần 2 theo backoff bình thường.

## ConnectFailReason → retry strategy

Mỗi transition khi fail set một `ConnectFailReason` → relay sang S3 (byte thứ 5 của `StatusPayload`), C5 quyết định retry.

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

## Interaction state (voice turn) — C5 owns, S3 mirrors

```
IDLE → LISTENING → PROCESSING → SPEAKING → IDLE
        (press /     (release /    (TTS đầu    (S3 SPEAK_DONE /
         wake word)    cap 30s,      tiên từ      audio_stop /
                       gửi "END")    server)      watchdog 15s)
SPEAKING|PROCESSING → LISTENING   (barge-in: bấm nút / wake word mới)
PROCESSING → IDLE                 (audio_stop / watchdog 20s)
```

Luồng một turn:

1. **Trigger** — nút trên C5, hoặc S3 (wake word) gửi `CONTROL_CMD START [src:1]` (`InputSource`: BUTTON/WAKEWORD; không payload = BUTTON cũ). C5 gửi text `"START"` lên server, vào `LISTENING`.
2. **Kết thúc nghe** — nhả nút hoặc watchdog 30s: C5 gửi `"END"`, vào **`PROCESSING`** (không phải IDLE — gate nhận TTS trong `NetworkManager::onBinary` chỉ mở khi PROCESSING/SPEAKING).
3. **Server trả lời** — `set_emotion` + `tts_play` (JSON) rồi stream Opus binary. Binary đầu tiên: C5 `PROCESSING → SPEAKING`.
4. **Kết thúc nói** — S3 phát hết audio (drain) → tự về IDLE local → gửi `CONTROL_CMD SPEAK_DONE` → C5 đóng speaking session, về `IDLE`.
5. **Không có trả lời** — server gửi `audio_stop` (STT rỗng / TTS fail) → C5 về IDLE ngay; backstop là watchdog PROCESSING 20s.

Watchdog phía C5 (`NetworkManager::checkInteraction`, tick 1s): `LISTENING > 30s` → auto-END; `PROCESSING > 20s` → IDLE; `SPEAKING` không có downlink 15s → IDLE. **Một turn luôn kết thúc.**

Validate Interaction trên C5 là **soft** (`isValidInteractionTransition`): transition lệch contract chỉ log WARN rồi vẫn áp dụng, để không desync relay sang S3.

S3 mirror **đầy đủ** mọi interaction state từ `STATUS_UPDATE` (kể cả IDLE khi đang PROCESSING — trước đây bị kẹt). Riêng LISTENING relay về dùng `src=UNKNOWN` để không echo START ngược lại C5.

`TRIGGERED`/`CANCELLING` còn trong enum cho tương thích nhưng không được set ở đâu (S3 coi như IDLE khi nhận qua relay).

## OTA state (8) — C5 manage, S3 display

```
IDLE → DOWNLOADING → VERIFYING → FLASHING → REBOOTING
   ↘ (push model: server `ota_available` bắt đầu tải ngay, bỏ qua CHECKING/AVAILABLE)
DOWNLOADING/VERIFYING/FLASHING ↘ FAILED → (re-push) DOWNLOADING   // retry không cần reboot
```

`CHECKING`/`AVAILABLE` vẫn trong enum cho đường poll `/ota/check` (app), nhưng đường push WS đi thẳng `IDLE → DOWNLOADING`. `isValidOtaTransition` phải cho phép `IDLE→DOWNLOADING` và `FAILED→DOWNLOADING` — nếu thiếu, mọi `setOtaState` bị từ chối → S3 + server không nhận được trạng thái.

Relay 2 hướng:
- **→ S3**: C5 `subscribeOta` → UART `OTA_STATUS [state:1][progress:1]`; progress DOWNLOADING theo bước ≥5% (`OtaManager::onProgress`). S3 `onOtaStatus` → `setOtaState` + `DisplayManager::handleOtaStatus`.
- **→ Server**: cùng callback gọi `NetworkManager::sendOtaProgress(percent, phase)` → WS `ota_progress {percent, phase}` (phase = `state::otaPhaseName`). Server `ws_manager._advance_ota_history` map phase → `OTAHistory.status` (`rebooting`→`completed`, `failed`→`failed`) + forward app.

Wrong-chip guard: `ota_available` mang `model`; `handleOtaAvailable` bỏ qua nếu `model` ≠ `app_meta::DLuniCE_MODEL` (firmware S3 chưa có đường OTA riêng, không được flash nhầm lên C5).

## Emotion — MỘT đường duy nhất qua DEVICE_CMD

Server chọn 1 trong 45 emotion key → C5 forward **nguyên chuỗi key** qua UART `DEVICE_CMD SET_EMOTION` → S3 map key→enum (bookkeeping trong StateManager) rồi `SceneManager::showEmotion(key)` render đúng category.

Byte emotion trong `STATUS_UPDATE` chỉ là bookkeeping của C5 (enum 16 giá trị, key lạ → NEUTRAL) — **S3 không dùng nó để render** (trước đây nó clobber scene đúng về "normal").

Enum 16: NEUTRAL, HAPPY, SAD, ANGRY, CONFUSED, EXCITED, CALM, THINKING, DISGUSTED, NERVOUS, EMBARRASSED, CURIOUS, ANNOYED, COOL, SUSPICIOUS, DETERMINED.

## StateEvent → server

`StateManager` (C5) phát `StateEvent { type (CONNECTION|INTERACTION|OTA|SYSTEM|POWER|EMOTION), old, new, fail_reason, timestamp }`. `subscribeAll` → `NetworkManager::sendStateUpdate` → server `state_update` (cache Redis `device:state:*` + DB `last_state` cho app). Tự drop khi offline.

## UART STATUS_UPDATE

`StatusPayload = [interaction:1][connection:1][system:1][emotion:1][fail_reason:1]` — gửi khi interaction/connection/system/emotion đổi + heartbeat 3s. S3 parser chấp nhận frame 4 byte cũ (fail_reason mặc định NONE) để cặp firmware lệch version vẫn chạy được.
