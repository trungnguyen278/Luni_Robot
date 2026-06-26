# Wake word "Hey Luni" — train FREE on Colab (microWakeWord)

Luni dùng wake word **on-device** trên ESP32-S3 (nơi có mic + 8MB PSRAM) để thay
nút nhấn. Vì **custom wake word của ESP-SR (Espressif) KHÔNG miễn phí**, ta dùng
**microWakeWord** (mã nguồn mở, TFLite-Micro) — train custom miễn phí trên Google
Colab, chính là cơ chế ESPHome dùng.

> Audio pipeline đã là **16 kHz mono** (xem `lib/audio/AudioConfig.hpp`) nên KHÔNG
> phải downsample — feed thẳng PCM mic vào detector.

## 0. CÔNG THỨC ĐÃ CHẠY ĐƯỢC (2026-06) — train lại / train từ khác

> **Đã chuyển "Hi Luni" → "Hey Luni"** ("Hi" quá ngắn → dễ kích nhầm/sót; "Hey" nguyên âm dài
> hơn nên detector phân biệt tốt hơn). **Train + nhúng XONG (Colab T4, 2026-06-26):**
> `lib/wakeword/model_hey_luni.h` (60896 byte, ký hiệu `g_hey_luni_model`, sha256
> `7882068ee72520b8…`); firmware đã trỏ sang model này; `DETECT_THRESHOLD=0.75` chọn từ bảng mục 7
> (recall **0.975** @ **0.37 false-accept/giờ** trên 5.3h nhiễu tiệc DiPCo); `STRIDE_MS=10`.
> Phiên âm Piper = `"hey loonie"`. **Còn lại: flash S3 + test/tinh chỉnh ngưỡng trên phần cứng thật.**
> (Model "Hi Luni" cũ đã gỡ.)

**Muốn train lại hoặc đổi từ khác → dùng notebook có sẵn: [`tools/train_wakeword.ipynb`](../tools/train_wakeword.ipynb)**

1. Lên <https://colab.research.google.com> → **File > Upload notebook** → chọn `tools/train_wakeword.ipynb`.
2. **Runtime > Change runtime type > T4 GPU**.
3. Sửa ô CONFIG: `WAKE_PHRASE` (vd `"hi bo"`), `MODEL_SLUG` (vd `hi_bo`). Dùng cách viết phiên âm nếu phát âm sai.
4. **Runtime > Run all** (~25–35 phút). Nghe thử 1 mẫu ở mục 2 trước khi để chạy tiếp.
5. Cuối cùng notebook tự tải về `model_<slug>.h` + `.tflite`. Copy `model_<slug>.h` vào `esp32-s3/lib/wakeword/`,
   rồi xem bảng ngưỡng ở mục 7 để chỉnh `DETECT_THRESHOLD` (chọn cutoff có recall cao mà FA/giờ ≈ 0).

**Các “bẫy” đã vá sẵn trong notebook** (Colab 2026, TF 2.20 + datasets 4.x):

- `datasets>=4` trả `torchcodec.AudioDecoder` (vỡ `Clips` của microWakeWord) → ghim `datasets<4.0`.
- TF 2.20 trả metric là `ndarray` còn code gọi `.numpy()` → vá `train.py`/`test.py` (`np.asarray(...)`).
- AudioSet/FMA trong notebook gốc bị gated/hỏng → thay bằng **ESC-50** làm nhiễu nền.
- `validate_nonstreaming` (lúc train) nạp cả tập ambient “split” → **OOM ~13GB RAM** → **trim** `validation_ambient` còn 1 entry.
- Bảng faph dựng sẵn của microWakeWord ra `nan` → notebook **tự tính lại** recall + false-accepts/giờ (mục 7).
- Frontend pymicro-features cố định **10ms hop / 30ms window / 40 kênh / 16kHz** → firmware phải để `STRIDE_MS=10`.

**Trạng thái trên thiết bị: ĐÃ TÍCH HỢP (2026-06).** `esp-tflite-micro` đã thêm vào
`src/idf_component.yml`; inference + microfrontend viết xong trong `lib/wakeword/WakeWordDetector.cpp`;
mic-tap always-on nối ở `AudioManager` + `DeviceProfile` (section 4c); `-DLUNI_ENABLE_WAKEWORD`
đã bật trong `platformio.ini`. **Không còn nút bấm trên mạch → wake word là trigger thoại duy nhất.**
Còn lại: **flash + tinh chỉnh `DETECT_THRESHOLD` trên phần cứng thật** (xem mục 7) — chưa test on-device.

---

## 1. Train model (FREE, Google Colab)

1. Mở repo **microWakeWord**: <https://github.com/kahrendt/microWakeWord> → notebook
   `basic_training_notebook.ipynb` trên **Colab** (chọn Runtime GPU — miễn phí).
2. Đặt cụm từ: `"Hey Luni"` (viết phiên âm `"hey loonie"` để Piper đọc đúng "loo-nee").
3. Notebook sẽ:
   - sinh **mẫu dương** bằng **Piper TTS** (nhiều giọng) + augment (noise, pitch),
   - lấy **mẫu âm nền/negative** từ dataset có sẵn,
   - train model **streaming** (inception nhỏ).
4. Xuất ra: `*.tflite` (vài chục KB) + manifest JSON (ngưỡng, stride).

## 2. Nhúng model vào firmware

```bash
# chuyển .tflite -> C array
xxd -i hey_luni.tflite > esp32-s3/lib/wakeword/model_hey_luni.h
```

Cập nhật ngưỡng trong `esp32-s3/lib/wakeword/WakeWordConfig.hpp` theo manifest
(`DETECT_THRESHOLD`, `WINDOW_MS`, `STRIDE_MS`). (File config đã chuyển từ `src/config/`
vào `lib/wakeword/` cùng model + detector để lib tự chứa — không include ngược vào `src/`.)

## 3. Tích hợp inference — ĐÃ LÀM (`lib/wakeword/WakeWordDetector.cpp`)

Bám đúng pipeline microWakeWord của ESPHome để model train trên Colab chạy y hệt on-device:

1. Component `esp-tflite-micro: "*"` (`src/idf_component.yml`) — **bundle sẵn** audio
   microfrontend (`tensorflow/lite/experimental/microfrontend/lib`) + op resource-variable
   cho model streaming.
2. `init()`: `FrontendConfig` (16 kHz, 30 ms/10 ms, 40 kênh, band 125–7500, noise-reduction
   + PCAN + log-scale = hằng số microWakeWord), `tflite::GetModel(g_hey_luni_model)`,
   `MicroMutableOpResolver<20>` (CallOnce, VarHandle, Reshape, ReadVariable, StridedSlice,
   Concatenation, AssignVariable, Conv2D, Mul, Add, Mean, FullyConnected, Logistic, Quantize,
   DepthwiseConv2D, AveragePool2D, MaxPool2D, Pad, Pack, SplitV), `MicroResourceVariables::Create(...,20)`,
   tensor arena **64 KB trong PSRAM**; đọc `stride = input->dims[1]` runtime.
3. `feed(pcm16k, n)`: dồn mẫu → `FrontendProcessSamples` (mỗi 10 ms → 1 frame 40 kênh) →
   quantize int8 `((v*256)+333)/666 − 128` → ghép `stride` frame → `Invoke` → prob uint8 →
   cửa sổ trượt `MOVING_AVG_FRAMES` (`sum > cutoff*N`) + refractory `REFRACTORY_MS` → `cb_()`.
   Tự reset khi feed gián đoạn > 200 ms (lúc listening/speaking).

## 4. Nối mic vào detector — ĐÃ LÀM (always-on)

`AudioManager::setWakeWordTap(cb)` cài tap + bật **mic always-on**. `micReadLoop` (Core 1):
IDLE → `startCapture` (idempotent) + `wake_tap_(pcm,n)` (bỏ qua khi `speaking` để TTS không
tự kích); LISTENING → uplink như cũ; `pauseListening`/`stopListening` không tắt mic khi
always-on. `DeviceProfile` 4c cài tap gọi `s_wakeword.feed` sau khi `init()` OK.

## 5. Bật — ĐÃ BẬT

`-DLUNI_ENABLE_WAKEWORD` đã có trong `esp32-s3/platformio.ini` (cạnh `-DLUNI_ENABLE_CAMERA`).

## Luồng khi phát hiện

`WakeWordDetector` → `onDetected()` (đã wire ở `DeviceProfile.cpp` section 4c) →
`setInteractionState(LISTENING, WAKEWORD)` → S3 mở mic + gửi `CONTROL_CMD START`
sang C5 → C5 báo server bắt đầu phiên thoại. (Giống hệt đường nút nhấn cũ, chỉ đổi
nguồn kích hoạt.)

## Vì sao KHÔNG dùng ESP-SR WakeNet

WakeNet built-in chỉ có sẵn vài wake word tiếng Anh/Trung; muốn custom "Hey Luni"
phải đặt **Espressif train (mất phí/đăng ký)**. microWakeWord cho train custom
**miễn phí** trên Colab → đúng yêu cầu.
