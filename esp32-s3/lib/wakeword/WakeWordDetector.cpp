#include "WakeWordDetector.hpp"
#include "esp_log.h"

static const char* TAG = "WakeWord";

#ifdef LUNI_ENABLE_WAKEWORD

#include "WakeWordConfig.hpp"
#include "model_hi_luni.h"

#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/micro_resource_variable.h"
#include "tensorflow/lite/micro/micro_allocator.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"

#include <cstring>
#include <vector>
#include <new>

namespace {

// ---------------------------------------------------------------------------
// microWakeWord on-device pipeline (matches ESPHome's micro_wake_word so the
// Colab-trained "Hi Luni" model behaves the same on device):
//   PCM 16k → microfrontend (40-ch log-mel spectrogram, 30ms win / 10ms hop)
//           → int8 quantize → streaming TFLM model (per-stride invoke, keeps
//             internal state via resource variables) → uint8 probability
//           → sliding-window average vs cutoff + refractory.
// Frontend / quantization constants below are microWakeWord's fixed values;
// do NOT change them independently of the trained model.
// ---------------------------------------------------------------------------

// int8 feature quantization (microWakeWord preprocessor)
constexpr int   kValueScale = 256;
constexpr int   kValueDiv   = 666;     // ≈ 25.6 * 26.0
constexpr int   kInt8Min    = -128;
constexpr int   kInt8Max    = 127;

// Reset spectrogram/probability state if feed() resumes after a gap this long
// (i.e. we were listening/speaking and stopped feeding the detector).
constexpr uint32_t kFeedGapResetMs = 200;

constexpr int kFeat = WakeWordConfig::FEATURE_CHANNELS;   // 40

static inline uint32_t nowMs()
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

struct WakeImpl {
    // microfrontend
    FrontendState frontend{};

    // TFLM — pointers/objects live for the program; arenas in PSRAM.
    const tflite::Model*               model = nullptr;
    tflite::MicroInterpreter*          interp = nullptr;
    tflite::MicroResourceVariables*    resources = nullptr;
    TfLiteTensor*                      input = nullptr;
    TfLiteTensor*                      output = nullptr;
    uint8_t*                           tensor_arena = nullptr;
    uint8_t                            var_arena[1024];   // matches ESPHome

    int     stride = 1;          // feature frames per Invoke (input dims[1])
    int     cur_step = 0;        // frames accumulated toward the next Invoke

    // sliding-window detector
    std::vector<uint8_t> probs;  // ring of recent probabilities (0..255)
    size_t  prob_head = 0;
    size_t  prob_count = 0;
    uint8_t cutoff_u8 = 0;       // DETECT_THRESHOLD * 255
    uint32_t last_detect_ms = 0;
    uint32_t last_feed_ms = 0;

    // sample buffer feeding the frontend (frontend consumes in step chunks)
    std::vector<int16_t> sample_buf;
};

void resetState(WakeImpl* w)
{
    FrontendReset(&w->frontend);
    w->cur_step = 0;
    w->prob_head = 0;
    w->prob_count = 0;
    w->sample_buf.clear();
    // last_detect_ms intentionally kept so a reset doesn't bypass refractory.
}

// Push a probability into the ring and decide if the wake word fired.
bool pushAndDetect(WakeImpl* w, uint8_t prob)
{
    const size_t N = (size_t)WakeWordConfig::MOVING_AVG_FRAMES;
    if (w->probs.size() < N) w->probs.resize(N, 0);

    w->probs[w->prob_head] = prob;
    w->prob_head = (w->prob_head + 1) % N;
    if (w->prob_count < N) w->prob_count++;

    if (w->prob_count < N) return false;  // window not full yet

    // refractory
    if (w->last_detect_ms != 0 &&
        (nowMs() - w->last_detect_ms) < (uint32_t)WakeWordConfig::REFRACTORY_MS) {
        return false;
    }

    uint32_t sum = 0;
    for (size_t i = 0; i < N; ++i) sum += w->probs[i];

    // avg > cutoff  ⇔  sum > cutoff * N  (all in 0..255 units)
    if (sum > (uint32_t)w->cutoff_u8 * (uint32_t)N) {
        w->last_detect_ms = nowMs();
        w->prob_count = 0;  // clear window so we don't re-fire on the same audio
        w->prob_head = 0;
        return true;
    }
    return false;
}

} // namespace

bool WakeWordDetector::init()
{
    if (ready_) return true;

    auto* w = new (std::nothrow) WakeImpl();
    if (!w) {
        ESP_LOGE(TAG, "WakeImpl alloc failed");
        return false;
    }

    // --- microfrontend: 16 kHz, 30 ms / 10 ms, 40 channels (microWakeWord) ---
    FrontendConfig cfg{};
    cfg.window.size_ms                       = WakeWordConfig::WINDOW_MS;   // 30
    cfg.window.step_size_ms                  = WakeWordConfig::STRIDE_MS;   // 10
    cfg.filterbank.num_channels              = kFeat;                       // 40
    cfg.filterbank.lower_band_limit          = 125.0f;
    cfg.filterbank.upper_band_limit          = 7500.0f;
    cfg.noise_reduction.smoothing_bits       = 10;
    cfg.noise_reduction.even_smoothing       = 0.025f;
    cfg.noise_reduction.odd_smoothing        = 0.06f;
    cfg.noise_reduction.min_signal_remaining = 0.05f;
    cfg.pcan_gain_control.enable_pcan        = 1;
    cfg.pcan_gain_control.strength           = 0.95f;
    cfg.pcan_gain_control.offset             = 80.0f;
    cfg.pcan_gain_control.gain_bits          = 21;
    cfg.log_scale.enable_log                 = 1;
    cfg.log_scale.scale_shift                = 6;

    if (!FrontendPopulateState(&cfg, &w->frontend, 16000)) {
        ESP_LOGE(TAG, "FrontendPopulateState failed");
        delete w;
        return false;
    }

    // --- TFLM model ---
    w->model = tflite::GetModel(g_hi_luni_model);
    if (w->model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "model schema %lu != %d",
                 (unsigned long)w->model->version(), TFLITE_SCHEMA_VERSION);
        FrontendFreeStateContents(&w->frontend);
        delete w;
        return false;
    }

    // Tensor arena in PSRAM (model + activations). Bump TENSOR_ARENA_BYTES if
    // AllocateTensors() fails.
    w->tensor_arena = (uint8_t*)heap_caps_malloc(
        WakeWordConfig::TENSOR_ARENA_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!w->tensor_arena) {
        ESP_LOGE(TAG, "tensor arena alloc (%d B PSRAM) failed",
                 WakeWordConfig::TENSOR_ARENA_BYTES);
        FrontendFreeStateContents(&w->frontend);
        delete w;
        return false;
    }

    // Op set + resource-variable count match ESPHome's streaming model (20).
    static tflite::MicroMutableOpResolver<20> resolver;
    static bool resolver_built = false;
    if (!resolver_built) {
        resolver.AddCallOnce();
        resolver.AddVarHandle();
        resolver.AddReshape();
        resolver.AddReadVariable();
        resolver.AddStridedSlice();
        resolver.AddConcatenation();
        resolver.AddAssignVariable();
        resolver.AddConv2D();
        resolver.AddMul();
        resolver.AddAdd();
        resolver.AddMean();
        resolver.AddFullyConnected();
        resolver.AddLogistic();
        resolver.AddQuantize();
        resolver.AddDepthwiseConv2D();
        resolver.AddAveragePool2D();
        resolver.AddMaxPool2D();
        resolver.AddPad();
        resolver.AddPack();
        resolver.AddSplitV();
        resolver_built = true;
    }

    tflite::MicroAllocator* var_alloc =
        tflite::MicroAllocator::Create(w->var_arena, sizeof(w->var_arena));
    w->resources = tflite::MicroResourceVariables::Create(var_alloc, 20);

    w->interp = new (std::nothrow) tflite::MicroInterpreter(
        w->model, resolver, w->tensor_arena,
        WakeWordConfig::TENSOR_ARENA_BYTES, w->resources);
    if (!w->interp) {
        ESP_LOGE(TAG, "interpreter alloc failed");
        heap_caps_free(w->tensor_arena);
        FrontendFreeStateContents(&w->frontend);
        delete w;
        return false;
    }

    if (w->interp->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors failed (arena too small or missing op)");
        delete w->interp;
        heap_caps_free(w->tensor_arena);
        FrontendFreeStateContents(&w->frontend);
        delete w;
        return false;
    }

    w->input  = w->interp->input(0);
    w->output = w->interp->output(0);

    // input shape [1, stride, 40] int8
    if (w->input->dims->size != 3 ||
        w->input->dims->data[0] != 1 ||
        w->input->dims->data[2] != kFeat ||
        w->input->type != kTfLiteInt8) {
        ESP_LOGE(TAG, "unexpected input dims/type (size=%d d0=%d d2=%d type=%d)",
                 w->input->dims->size, w->input->dims->data[0],
                 w->input->dims->data[2], (int)w->input->type);
        delete w->interp;
        heap_caps_free(w->tensor_arena);
        FrontendFreeStateContents(&w->frontend);
        delete w;
        return false;
    }
    w->stride = w->input->dims->data[1];
    if (w->stride < 1) w->stride = 1;

    w->cutoff_u8 = (uint8_t)(WakeWordConfig::DETECT_THRESHOLD * 255.0f);
    w->probs.assign((size_t)WakeWordConfig::MOVING_AVG_FRAMES, 0);
    w->sample_buf.reserve(960);

    impl_ = w;
    ready_ = true;
    ESP_LOGI(TAG, "wake word ready (stride=%d, arena=%dKB PSRAM, cutoff=%u/255)",
             w->stride, WakeWordConfig::TENSOR_ARENA_BYTES / 1024, w->cutoff_u8);
    return true;
}

void WakeWordDetector::reset()
{
    if (!ready_ || !impl_) return;
    resetState(static_cast<WakeImpl*>(impl_));
}

void WakeWordDetector::feed(const int16_t* pcm16k, size_t samples)
{
    if (!ready_ || !impl_ || !pcm16k || samples == 0) return;
    auto* w = static_cast<WakeImpl*>(impl_);

    // Resumed after a feeding gap (we were listening/speaking) → start clean.
    const uint32_t t = nowMs();
    if (w->last_feed_ms != 0 && (t - w->last_feed_ms) > kFeedGapResetMs) {
        resetState(w);
    }
    w->last_feed_ms = t;

    // Accumulate, then let the frontend consume in 10ms (160-sample) steps.
    w->sample_buf.insert(w->sample_buf.end(), pcm16k, pcm16k + samples);

    size_t offset = 0;
    while (offset < w->sample_buf.size()) {
        size_t num_read = 0;
        FrontendOutput out = FrontendProcessSamples(
            &w->frontend,
            w->sample_buf.data() + offset,
            w->sample_buf.size() - offset,
            &num_read);
        if (num_read == 0) break;   // need more samples for the next step
        offset += num_read;

        if (out.size == 0) continue;  // step consumed, no new feature window yet

        // Quantize the 40 uint16 frontend values into the int8 input slot for
        // the current stride step.
        int8_t* dst = w->input->data.int8 + (w->cur_step * kFeat);
        for (int i = 0; i < kFeat && i < (int)out.size; ++i) {
            int v = ((int)out.values[i] * kValueScale + kValueDiv / 2) / kValueDiv
                    + kInt8Min;
            if (v < kInt8Min) v = kInt8Min;
            else if (v > kInt8Max) v = kInt8Max;
            dst[i] = (int8_t)v;
        }
        w->cur_step++;

        if (w->cur_step < w->stride) continue;  // need a full stride before invoke
        w->cur_step = 0;

        if (w->interp->Invoke() != kTfLiteOk) {
            ESP_LOGW(TAG, "Invoke failed");
            continue;
        }

        uint8_t prob = w->output->data.uint8[0];
        if (pushAndDetect(w, prob)) {
            ESP_LOGI(TAG, "wake word detected (prob=%u/255)", prob);
            if (cb_) cb_();
        }
    }

    // Drop the consumed prefix; keep the unconsumed tail for the next call.
    if (offset > 0) {
        w->sample_buf.erase(w->sample_buf.begin(), w->sample_buf.begin() + offset);
    }
}

#else  // wake word disabled

bool WakeWordDetector::init()
{
    ESP_LOGI(TAG, "wake word disabled (define LUNI_ENABLE_WAKEWORD to enable)");
    ready_ = false;
    return false;
}

void WakeWordDetector::feed(const int16_t* /*pcm16k*/, size_t /*samples*/) {}
void WakeWordDetector::reset() {}

#endif  // LUNI_ENABLE_WAKEWORD
