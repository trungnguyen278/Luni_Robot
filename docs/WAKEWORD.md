# Wake word "Hi Luni" — train FREE on Colab (microWakeWord)

Luni dùng wake word **on-device** trên ESP32-S3 (nơi có mic + 8MB PSRAM) để thay
nút nhấn. Vì **custom wake word của ESP-SR (Espressif) KHÔNG miễn phí**, ta dùng
**microWakeWord** (mã nguồn mở, TFLite-Micro) — train custom miễn phí trên Google
Colab, chính là cơ chế ESPHome dùng.

> Audio pipeline đã là **16 kHz mono** (xem `lib/audio/AudioConfig.hpp`) nên KHÔNG
> phải downsample — feed thẳng PCM mic vào detector.

## 0. CÔNG THỨC ĐÃ CHẠY ĐƯỢC (2026-06) — train lại / train từ khác

> **"Hi Luni" đã train + nhúng xong rồi**: `lib/wakeword/model_hi_luni.h` (60840 byte, ký hiệu
> `g_hi_luni_model`), và `src/config/WakeWordConfig.hpp` đã set `DETECT_THRESHOLD=0.60`,
> `STRIDE_MS=10`. Chất lượng: recall ~0.98, **0 lần kích nhầm/giờ** trên 5.3 giờ tiếng ồn tiệc.

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

**Còn lại để chạy trên thiết bị** (chưa làm — xem mục 3–4 bên dưới): thêm `esp-tflite-micro`, viết inference
trong `WakeWordDetector.cpp`, nối mic-tap `feed()`, bật `-DLUNI_ENABLE_WAKEWORD`.

---

## 1. Train model (FREE, Google Colab)

1. Mở repo **microWakeWord**: <https://github.com/kahrendt/microWakeWord> → notebook
   `basic_training_notebook.ipynb` trên **Colab** (chọn Runtime GPU — miễn phí).
2. Đặt cụm từ: `"Hi Luni"` (tiếng Việt cũng được — Piper tự sinh giọng).
3. Notebook sẽ:
   - sinh **mẫu dương** bằng **Piper TTS** (nhiều giọng) + augment (noise, pitch),
   - lấy **mẫu âm nền/negative** từ dataset có sẵn,
   - train model **streaming** (inception nhỏ).
4. Xuất ra: `*.tflite` (vài chục KB) + manifest JSON (ngưỡng, stride).

## 2. Nhúng model vào firmware

```bash
# chuyển .tflite -> C array
xxd -i hi_luni.tflite > esp32-s3/lib/wakeword/model_hi_luni.h
```

Cập nhật ngưỡng trong `esp32-s3/src/config/WakeWordConfig.hpp` theo manifest
(`DETECT_THRESHOLD`, `WINDOW_MS`, `STRIDE_MS`).

## 3. Tích hợp inference (TODO trong `WakeWordDetector.cpp`)

1. Thêm component vào `esp32-s3/src/idf_component.yml`:
   ```yaml
   esp-tflite-micro: "*"
   ```
2. Trong `WakeWordDetector::init()` (nhánh `#ifdef LUNI_ENABLE_WAKEWORD`):
   - nạp model (`tflite::GetModel(model_hi_luni)`), op resolver, interpreter,
     **tensor arena trong PSRAM**.
3. `feed(pcm16k, n)`:
   - dồn mẫu, dựng **spectrogram 40 kênh** (cửa sổ `WINDOW_MS`, hop `STRIDE_MS`),
   - chạy model mỗi stride; nếu `prob > DETECT_THRESHOLD` và đã qua `REFRACTORY_MS`
     → gọi `cb_()`.

## 4. Nối mic vào detector (điểm còn lại)

Mic được `AudioManager` đọc ở 16 kHz. Thêm một "tap": trong vòng đọc mic, gọi
`s_wakeword.feed(pcm, samples)` song song với đường Opus. (Detector chạy nhẹ; có
thể đặt ở Core 1 cùng audio hoặc Core 0.)

## 5. Bật

Thêm vào `esp32-s3/platformio.ini`:
```ini
build_flags =
    ...
    -DLUNI_ENABLE_WAKEWORD
```

## Luồng khi phát hiện

`WakeWordDetector` → `onDetected()` (đã wire ở `DeviceProfile.cpp` section 4c) →
`setInteractionState(LISTENING, WAKEWORD)` → S3 mở mic + gửi `CONTROL_CMD START`
sang C5 → C5 báo server bắt đầu phiên thoại. (Giống hệt đường nút nhấn cũ, chỉ đổi
nguồn kích hoạt.)

## Vì sao KHÔNG dùng ESP-SR WakeNet

WakeNet built-in chỉ có sẵn vài wake word tiếng Anh/Trung; muốn custom "Hi Luni"
phải đặt **Espressif train (mất phí/đăng ký)**. microWakeWord cho train custom
**miễn phí** trên Colab → đúng yêu cầu.
