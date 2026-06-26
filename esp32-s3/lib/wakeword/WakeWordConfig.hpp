#pragma once
#include <cstdint>

// =============================================================================
// Wake-word tunables (microWakeWord on esp-tflite-micro). The audio is the
// native 16 kHz mic stream (see AudioConfig — no downsampling needed).
// The model itself ("Hey Luni") is trained for FREE on Colab and embedded as a
// C array — see docs/WAKEWORD.md. Enable with -DLUNI_ENABLE_WAKEWORD.
//
// Co-located with the model + detector in lib/wakeword so the wake-word unit is
// self-contained (a lib component must not include back into src/).
// =============================================================================
namespace WakeWordConfig {

// --- Detection ---
// Tuned from the "Hey Luni" model's ROC over 5.3 h DiPCo/dinner-party ambient + 200
// positive clips (microWakeWord step-7 table; trained on Colab T4 2026-06-26):
//   cutoff 0.75 -> recall 0.975, 0.37 false-accepts/hour (best recall AND lowest FA).
//   0.50-0.70 -> same recall but 0.56 FA/h; 0.80 -> recall 0.96; 0.90 -> 0.935 (FA flat 0.37).
// Lower = easier to trigger; raise toward 0.85-0.95 if real-world false triggers occur.
static constexpr float DETECT_THRESHOLD  = 0.75f;  // model probability cutoff (0..1)
static constexpr int   MOVING_AVG_FRAMES = 5;      // smoothing window (matches training eval)
static constexpr int   REFRACTORY_MS     = 1500;   // ignore re-triggers for this long

// --- Spectrogram frontend (MUST match the trained model / pymicro-features) ---
// The C microfrontend is fixed at 16 kHz, 30 ms window, 10 ms hop, 40 channels.
static constexpr int   WINDOW_MS        = 30;     // feature window
static constexpr int   STRIDE_MS        = 10;     // feature hop (NOT 20 — frontend is 10 ms)
static constexpr int   FEATURE_CHANNELS = 40;     // mel-ish channels per frame

// --- TFLite-Micro ---
// Model C-array: g_hey_luni_model / g_hey_luni_model_len (lib/wakeword/model_hey_luni.h).
// Put the tensor arena in PSRAM; bump if AllocateTensors fails.
static constexpr int   TENSOR_ARENA_BYTES = 64 * 1024;

} // namespace WakeWordConfig
