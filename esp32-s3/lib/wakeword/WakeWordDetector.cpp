#include "WakeWordDetector.hpp"
#include "esp_log.h"

static const char* TAG = "WakeWord";

#ifdef LUNI_ENABLE_WAKEWORD
// ----------------------------------------------------------------------------
// microWakeWord integration point (esp-tflite-micro).
// TODO (see docs/WAKEWORD.md):
//   1) add `esp-tflite-micro` to idf_component.yml
//   2) embed the trained model C-array (model_hi_luni.h) + manifest
//   3) build the 40-channel spectrogram frontend over 16kHz frames
//      (WakeWordConfig::WINDOW_MS / STRIDE_MS)
//   4) run the streaming model each stride; if prob > DETECT_THRESHOLD and the
//      refractory window elapsed → call cb_()
// Until that is wired, init() reports not-ready so the trigger stays inactive.
// ----------------------------------------------------------------------------
bool WakeWordDetector::init()
{
    ESP_LOGW(TAG, "LUNI_ENABLE_WAKEWORD set but model not integrated yet "
                  "(see docs/WAKEWORD.md)");
    ready_ = false;
    return false;
}

void WakeWordDetector::feed(const int16_t* /*pcm16k*/, size_t /*samples*/)
{
    // no-op until the model is integrated
}

#else  // wake word disabled

bool WakeWordDetector::init()
{
    ESP_LOGI(TAG, "wake word disabled (define LUNI_ENABLE_WAKEWORD to enable)");
    ready_ = false;
    return false;
}

void WakeWordDetector::feed(const int16_t* /*pcm16k*/, size_t /*samples*/) {}

#endif  // LUNI_ENABLE_WAKEWORD
