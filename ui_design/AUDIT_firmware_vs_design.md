# AUDIT — Firmware UI (C++) vs Desired Design (JSX)

So sánh `esp32-s3/src/ui/**` (đang dùng) với `ui_design/**` (chuẩn). JSX là nguồn chuẩn; firmware là bên cần sửa.
Trục kiểm: **tone category · override tone variant · inventory variant (id/duration) · màu mắt · animation**.

Trạng thái: ✅ khớp · ⚠️ lệch nhỏ · ❌ phải sửa

---

## ✳️ TIẾN ĐỘ SỬA (cập nhật)

**Phase 1 — Màu: ĐÃ XONG & build pass (exit 0).**
- §1 dọn comment LUT ✅
- §3a 6 tone category scene (camera→white, gift→rose, smarthome→warm, shopping→green, delivery→orange, celebrate→cyan) ✅
- §3c per-variant tone: call-missed→RED, message-sent→ROSE ✅
- §3d **mắt watcher = cyan ở cả 35 lệnh `drawPlacedEyes`** (toàn bộ 32 scene) ✅

**Phase 2 — đã làm phần bounded:**
- §3b đổi **13 id scene** khớp JSX (alarm-bell, notif-cards, birthday-cake, reminder-triple, video-watch, reading-book, home-lights, shopping-bag, lock-bio, delivery-route, flashlight-sweep, podcast-onair, stopwatch-lap) ✅ — đã verify **không host nào** tham chiếu id cũ.
- §3e duration camera 12000→14000 ✅

**Phase 2 — Fidelity animation (đã làm phần lớn, build pass nhiều đợt):**

Quy tắc áp dụng: **rewrite khi animation khác loại/cấu trúc hoặc thiếu phần tử chính**; GIỮ khi cùng concept dù khác hằng số. Phát hiện: variant từ `emotion-list` (base) + các **pack Luni** (sick/cold/calm/playful/hot/lonely/grateful/brave/dreamy/awe — `render_playful` khớp gần khít) là **port trung thành → giữ**. Variant lệch tập trung ở nhóm viết sớm.

- **Đã rewrite (~37 variant)**: normal (8 extras), happy (4), surprised (flash/mild), love (blush/shower), excited (giddy/shockwave), sleepy-nod, sleeping-deep, crying-sob, loading-bricks, dizzy-stars, dead-glitch, focused (laser/scan), proud (puffed/glow), error (bang/noconn), charging-bolt, mischievous (grin/laugh), disgusted-yuck, annoyed-flat.
- **Đã đối chiếu & GIỮ (faithful concept)**: confused, bored, listening, thinking, wink, scared, smug, shy, greet, hungry, mute, curious, + toàn bộ 10 pack Luni.

**CÒN LẠI (polish, ưu tiên thấp):**
- Nhóm **"more"** bản đơn giản hoá còn thiếu phụ kiện so JSX: `nervous` (miệng lo lắng/pupil), `embarrassed` (blush kẻ sọc, thanh che ngón tay, răng nghiến), `cool` (kính râm shades), `suspicious`, `determined`, `disgusted-gag/sour`, `annoyed-twitch/sigh`. Concept đúng, thiếu chi tiết accessory.
- **Scene ACT**: animation phase-based của firmware kể đúng câu chuyện + đã đúng màu/mắt/id/inventory; chi tiết từng frame trong pha ACT chưa đối chiếu pixel với JSX (khối lớn, ưu tiên thấp vì màu/cấu trúc đã khớp).

---

## 0. Kết luận nhanh

- **Hạ tầng màu khớp**: 9-tone LUT đúng giá trị; `forEmotion()` (eye=cyan, accent=tone) và `forScene()` (eye=accent=tone, cho **props**) đúng ý đồ; STATUS → `forEmotion` (eye=cyan) đúng.
- **47 emotion**: inventory + id + tone **khớp 100%**. Chỉ còn đối chiếu độ trung thực animation (Phase 2).
- **2 status**: tone category + per-variant + id khớp (network khớp tuyệt đối; boot khớp hiệu dụng).
- **32 scene**: nơi tập trung gần như toàn bộ khác biệt → xem mục 3.

**Phát hiện màu hệ thống lớn nhất**: trong [scene-arc.jsx](scene-arc.jsx) mắt robot (LuniFace watcher) **luôn cyan** (`LUNI_EYE_COLOR='#5be9ff'`). Firmware truyền **tone** (hoặc `colors.eye`) vào `drawPlacedEyes(...)` ở hầu hết scene → mắt watcher bị nhuộm sai. Chỉ `camera` và `message` đang đúng cyan. → **Đây chính là "rất nhiều khác màu sắc".**

---

## 1. Palette / hạ tầng màu — ✅ (chỉ dọn comment)

- 9 tone trong [ColorSystem.hpp](../esp32-s3/lib/display/ColorSystem.hpp) đúng giá trị RGB565. Comment "(adjusted: 0xFB54/0x7F52)" ở dòng 24,27 **sai lệch** → xoá (số hiện tại `0xFB53`, `0x7F51` đã đúng theo công thức `rgb565()`).
- `COLOR_BG=0x0841` (#06090d) đúng.

## 2. Emotions (47) — ✅ inventory khớp 100%

Mọi category emotion: **đúng số variant, đúng id, đúng tone**. Ví dụ đã đối chiếu: normal 16/16, happy 8/8, sad 6/6, listening 7/7, thinking 7/7, các pack Luni (sick/cold/calm/playful/hot/lonely/grateful/brave/dreamy/awe) đủ. Tone category 47/47 khớp [emotion-color.jsx](emotion-color.jsx) + tone inline pack.

→ Phase 2 chỉ cần đối chiếu **fidelity animation** từng variant (chuyển động/easing/số hạt) so với `render(t)` JSX; **không** thêm/bớt variant.

## 2b. Status (2) — ✅

- **network**: 7 variant, per-variant tone khớp tuyệt đối (scan cyan, connect cyan, retry orange, offline red, bt-scan purple, bt-paired purple, server-error red). ⚠️ label `bt-scan` = "BLE pairing" (JSX "BT searching") — chỉ cosmetic.
- **boot**: 4 variant id khớp (poweron/credits/checks-personal/ready-personal), tất cả cyan ở cả hai bên (hiệu dụng khớp). ⚠️ tone category fw=CYAN vs JSX `SCENE_TONE.boot='red'` nhưng JSX override mọi variant về cyan → kết quả như nhau, không cần đổi.

---

## 3. Scenes (32) — ❌ nơi tập trung khác biệt

### 3a. Sai TONE category (props sẽ sai màu) — sửa `CategoryDef.tone`
| Scene | FW tone | JSX tone | Sửa |
|---|---|---|---|
| camera | CYAN | **white** | TONE_WHITE |
| gift | WARM | **rose** | TONE_ROSE |
| smarthome | GREEN | **warm** | TONE_WARM |
| shopping | ORANGE | **green** | TONE_GREEN |
| delivery | WARM | **orange** | TONE_ORANGE |
| celebrate | WARM | **multi** | TONE_CYAN (base của 'multi'; confetti đã dùng `colors.tones[]`) |

### 3b. Sai VARIANT ID (hợp đồng host) — đổi id cho khớp JSX
| FW id | JSX id |
|---|---|
| alarm-main | **alarm-bell** |
| notification-main | **notif-cards** |
| birthday-main | **birthday-cake** |
| reminder-main | **reminder-triple** |
| video-main | **video-watch** |
| reading-main | **reading-book** |
| smarthome-main | **home-lights** |
| shopping-main | **shopping-bag** |
| lock-main | **lock-bio** |
| delivery-main | **delivery-route** |
| flashlight-main | **flashlight-sweep** |
| podcast-main | **podcast-onair** |
| stopwatch-main | **stopwatch-lap** |

(Các scene đã khớp id: clock-tower, music-jam, timer-countdown, cooking-pot, walking-scroll, celebrate-grand, night-fall, fitness-run, nav-turn, gift-unwrap, coffee-pour, plant-grow, morning-rise, gaming-play, calendar-mark, camera-snap, weather-window, call-*, message-*, moon-*.) → đổi id phải rà host call-sites + `SceneManager`.

### 3c. Multi-case: thiếu variant / thiếu per-variant tone
| Scene | FW hiện có | JSX cần | Việc |
|---|---|---|---|
| **weather** | weather-window (1) | window(cyan), cloudy(cyan), rainy(blue), storm(purple), snow(white), fog(blue), windy(cyan) | **thêm 6 variant** + render riêng (Phase 2) + per-variant tone |
| **call** | incoming, outgoing, missed (tone NONE) | incoming(green), outgoing(green), **missed(red)** | đặt `VariantDef.tone=TONE_RED` cho call-missed |
| **message** | chat, incoming, sent (tone NONE) | chat(cyan), incoming(cyan), **sent(rose)** | đặt `VariantDef.tone=TONE_ROSE` cho message-sent |
| **moon** | 8 phase, tone purple/cyan/cyan/warm/warm/warm/blue/purple | giống hệt | ✅ id + tone khớp |

### 3d. MÀU MẮT WATCHER — ❌ phải về cyan ở MỌI scene
`drawPlacedEyes(... eyeColor ...)` phải nhận `TONE_LUT[TONE_CYAN]`. Hiện trạng đã kiểm:
- **Sai (đang dùng tone)**: alarm(RED), celebrate(WARM), cooking(`colors.eye`), coffee(`colors.eye`), delivery(WARM), fitness(RED), flashlight(WARM), lock(GREEN), morning(WARM), call(GREEN/GREEN/RED 3 chỗ).
- **Đúng (cyan)**: camera, message (2 chỗ).
- **Chưa kiểm → phải rà nốt**: clock, weather, music, timer, walking, night, moon(×8), gaming, calendar, navigation, plant, gift, notification, birthday, reminder, video, reading, smarthome, shopping, podcast, stopwatch.
→ Sửa đồng loạt: thay mọi đối số eyeColor của `drawPlacedEyes` thành `TONE_LUT[TONE_CYAN]`.

### 3e. Duration lệch (đối chiếu khi làm Phase 2)
- **camera**: fw 12000 vs JSX **14000** ❌. (alarm 12000✓, notification 12000✓, music 12000✓, weather-window 12000✓ — phần còn lại spot-check khi rebuild.)

---

## 4. Thứ tự ưu tiên sửa
1. **Phase 1 màu (bounded)**: dọn LUT comment (§1) → 6 tone category scene (§3a) → call-missed/message-sent tone (§3c) → mắt watcher = cyan toàn scene (§3d).
2. **Phase 2 frame**: đổi 13 id scene (§3b) + rà host call-sites → thêm 6 weather variant (§3c) → sửa duration lệch (§3e) → tái dựng animation từng scene & đối chiếu fidelity emotion (§2) theo `render(t)` JSX.
