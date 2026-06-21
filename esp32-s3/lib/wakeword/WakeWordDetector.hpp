#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>

// =============================================================================
// WakeWordDetector — on-device "Hi Luni" wake word (microWakeWord / TFLite-Micro)
// running on the ESP32-S3. Replaces the push-to-talk button.
//
// Audio is the native 16 kHz mono mic stream (AudioConfig::SAMPLE_RATE) so no
// resampling is needed. feed() is called with mic PCM; on detection the
// onDetected() callback fires (→ enter LISTENING with InputSource::WAKEWORD).
//
// The actual inference is compiled only with -DLUNI_ENABLE_WAKEWORD and requires
// the esp-tflite-micro component + the trained model (see docs/WAKEWORD.md).
// Without the flag this is a no-op stub so the rest of the firmware builds.
// =============================================================================
class WakeWordDetector
{
public:
    using DetectCb = std::function<void()>;

    bool init();
    void feed(const int16_t* pcm16k, size_t samples);   // mic audio, 16kHz mono
    // Drop accumulated spectrogram + probability state. Call when the mic feed
    // pauses (listening/speaking) so the next idle window starts clean and a
    // stale partial frame can't cause a false trigger.
    void reset();
    void onDetected(DetectCb cb) { cb_ = std::move(cb); }
    bool isReady() const { return ready_; }

private:
    bool     ready_ = false;
    DetectCb cb_;
    // Opaque TFLite-Micro + microfrontend state (only allocated when built with
    // LUNI_ENABLE_WAKEWORD); kept out of the header to avoid leaking TFLM into
    // every TU that includes this.
    void*    impl_ = nullptr;
};
