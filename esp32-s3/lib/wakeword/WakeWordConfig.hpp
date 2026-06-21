#pragma once
#include <cstdint>

// =============================================================================
// Wake-word tunables (microWakeWord on esp-tflite-micro). The audio is the
// native 16 kHz mic stream (see AudioConfig — no downsampling needed).
// The model itself ("Hi Luni") is trained for FREE on Colab and embedded as a
// C array — see docs/WAKEWORD.md. Enable with -DLUNI_ENABLE_WAKEWORD.
//
// Co-located with the model + detector in lib/wakeword so the wake-word unit is
// self-contained (a lib component must not include back into src/).
// =============================================================================
namespace WakeWordConfig {

// --- Detection ---
// Tuned from the trained model's ROC over 5.3 h of dinner-party/DiPCo ambient +
// 200 positive clips (microWakeWord, see docs/WAKEWORD.md):
//   cutoff 0.60 -> recall 0.98, 0 false-accepts/hour. Lower = easier to trigger
//   (0.50 ~ 0.2 FA/h); raise toward 0.80-0.90 if real-world false triggers occur.
static constexpr float DETECT_THRESHOLD  = 0.60f;  // model probability cutoff (0..1)
static constexpr int   MOVING_AVG_FRAMES = 5;      // smoothing window (matches training eval)
static constexpr int   REFRACTORY_MS     = 1500;   // ignore re-triggers for this long

// --- Spectrogram frontend (MUST match the trained model / pymicro-features) ---
// The C microfrontend is fixed at 16 kHz, 30 ms window, 10 ms hop, 40 channels.
static constexpr int   WINDOW_MS        = 30;     // feature window
static constexpr int   STRIDE_MS        = 10;     // feature hop (NOT 20 — frontend is 10 ms)
static constexpr int   FEATURE_CHANNELS = 40;     // mel-ish channels per frame

// --- TFLite-Micro ---
// Model C-array: g_hi_luni_model / g_hi_luni_model_len (lib/wakeword/model_hi_luni.h,
// 60840 bytes). Put the tensor arena in PSRAM; bump if AllocateTensors fails.
static constexpr int   TENSOR_ARENA_BYTES = 64 * 1024;

} // namespace WakeWordConfig
