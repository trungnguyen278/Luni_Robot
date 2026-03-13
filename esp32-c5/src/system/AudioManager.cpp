#include "AudioManager.hpp"
#include "AudioInput.hpp"
#include "AudioOutput.hpp"
#include "AudioCodec.hpp"
#include "esp_log.h"
#include <cstring>

static const char *TAG = "AudioManager";

AudioManager::AudioManager() = default;

AudioManager::~AudioManager()
{
    stop();
}

// === Dependency injection ===

void AudioManager::setInput(std::unique_ptr<AudioInput> in)   { input = std::move(in); }
void AudioManager::setOutput(std::unique_ptr<AudioOutput> out) { output = std::move(out); }
void AudioManager::setCodec(std::unique_ptr<AudioCodec> cdc)  { codec = std::move(cdc); }

void AudioManager::setVolume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    if (output) {
        output->setVolume(percent);
        ESP_LOGI(TAG, "Volume set to %u%%", (unsigned)percent);
    }
}

// === Init / Start / Stop ===

bool AudioManager::init()
{
    ESP_LOGI(TAG, "init()");

    if (!input || !output || !codec) {
        ESP_LOGE(TAG, "Missing input/output/codec");
        return false;
    }

    if (!input->init()) {
        ESP_LOGE(TAG, "Failed to init Audio Input");
        return false;
    }

    // Create stream buffers
    sb_mic_pcm     = xStreamBufferCreate(4 * 1024, 1);
    sb_mic_encoded = xStreamBufferCreate(32 * 1024, 1);
    sb_spk_pcm     = xStreamBufferCreate(8 * 1024, 1);
    sb_spk_encoded = xStreamBufferCreate(16 * 1024, 1);

    if (!sb_mic_pcm || !sb_mic_encoded || !sb_spk_pcm || !sb_spk_encoded) {
        ESP_LOGE(TAG, "Failed to create stream buffers");
        return false;
    }

    // Subscribe to interaction state
    sub_interaction_id = StateManager::instance().subscribeInteraction(
        [this](state::InteractionState s, state::InputSource src) {
            this->handleInteractionState(s, src);
        });

    ESP_LOGI(TAG, "AudioManager init OK");
    return true;
}

void AudioManager::start()
{
    if (started) return;
    started = true;
    ESP_LOGI(TAG, "start()");

    // Task doc mic (reads I2S -> sb_mic_pcm)
    xTaskCreatePinnedToCore(&AudioManager::micReadTaskEntry, "MicRead", 4096, this, 6, &mic_read_task, 0);
    // Task Stream mic (encode sb_mic_pcm -> sb_mic_encoded)
    xTaskCreatePinnedToCore(&AudioManager::micStreamTaskEntry, "MicStream", 8192, this, 5, &mic_stream_task, 0);
    // Task Nhan Audio (decode sb_spk_encoded -> sb_spk_pcm)
    xTaskCreatePinnedToCore(&AudioManager::audioRecvTaskEntry, "AudioRecv", 8192, this, 5, &audio_recv_task, 0);
    // Task phat audio (sb_spk_pcm -> I2S TX)
    xTaskCreatePinnedToCore(&AudioManager::spkPlayTaskEntry, "SpkPlay", 4096, this, 6, &spk_play_task, 0);
}

void AudioManager::stop()
{
    if (!started) return;
    started = false;
    ESP_LOGW(TAG, "stop()");
    stopAll();

    // Wait for tasks to exit
    vTaskDelay(pdMS_TO_TICKS(500));
    mic_read_task = nullptr;
    mic_stream_task = nullptr;
    audio_recv_task = nullptr;
    spk_play_task = nullptr;
}

bool AudioManager::allocateResources()
{
    if (sb_mic_pcm != nullptr) return true;
    sb_mic_pcm     = xStreamBufferCreate(4 * 1024, 1);
    sb_mic_encoded = xStreamBufferCreate(32 * 1024, 1);
    sb_spk_pcm     = xStreamBufferCreate(8 * 1024, 1);
    sb_spk_encoded = xStreamBufferCreate(16 * 1024, 1);
    return (sb_mic_pcm && sb_mic_encoded && sb_spk_pcm && sb_spk_encoded);
}

void AudioManager::freeResources()
{
    stop();
    auto del = [](StreamBufferHandle_t& h) { if (h) { vStreamBufferDelete(h); h = nullptr; } };
    del(sb_mic_pcm); del(sb_mic_encoded); del(sb_spk_pcm); del(sb_spk_encoded);
    ESP_LOGI(TAG, "Resources freed");
}

// === State handling ===

void AudioManager::handleInteractionState(state::InteractionState s, state::InputSource src)
{
    switch (s) {
    case state::InteractionState::LISTENING:   startListening(src); break;
    case state::InteractionState::PROCESSING:  pauseListening(); break;
    case state::InteractionState::SPEAKING:    startSpeaking(); break;
    case state::InteractionState::CANCELLING:
    case state::InteractionState::IDLE:        stopAll(); break;
    case state::InteractionState::SLEEPING:    stopAll(); setPowerSaving(true); break;
    default: break;
    }
}

// === Audio actions ===

void AudioManager::startListening(state::InputSource src)
{
    if (listening) return;
    ESP_LOGI(TAG, "Start listening");

    if (speaking) stopSpeaking();
    xStreamBufferReset(sb_spk_encoded);
    xStreamBufferReset(sb_spk_pcm);
    if (codec) codec->reset();

    current_source = src;
    listening = true;
    speaking = false;
    input->startCapture();
}

void AudioManager::pauseListening()
{
    if (!listening) return;
    ESP_LOGI(TAG, "Pause listening");
    input->stopCapture();
}

void AudioManager::stopListening()
{
    if (!listening) return;
    ESP_LOGI(TAG, "Stop listening");
    listening = false;
    input->stopCapture();
    if (sb_mic_pcm) xStreamBufferReset(sb_mic_pcm);
    if (sb_mic_encoded) xStreamBufferReset(sb_mic_encoded);
    if (codec) codec->reset();
}

void AudioManager::startSpeaking()
{
    if (speaking) return;
    ESP_LOGI(TAG, "Start speaking");
    speaking = true;
    if (spk_play_task) xTaskNotifyGive(spk_play_task);
}

void AudioManager::stopSpeaking()
{
    if (!speaking) return;
    ESP_LOGI(TAG, "Stop speaking");
    speaking = false;
    if (output) output->stopPlayback();
    if (sb_spk_encoded) xStreamBufferReset(sb_spk_encoded);
    if (sb_spk_pcm) xStreamBufferReset(sb_spk_pcm);
    if (codec) codec->reset();
}

void AudioManager::stopAll()  { stopListening(); stopSpeaking(); }
void AudioManager::setPowerSaving(bool enable) { power_saving = enable; if (enable) stopAll(); }

// === Task entries ===

void AudioManager::micReadTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->micReadLoop(); }
void AudioManager::micStreamTaskEntry(void* arg)  { static_cast<AudioManager*>(arg)->micStreamLoop(); }
void AudioManager::audioRecvTaskEntry(void* arg)   { static_cast<AudioManager*>(arg)->audioRecvLoop(); }
void AudioManager::spkPlayTaskEntry(void* arg)     { static_cast<AudioManager*>(arg)->spkPlayLoop(); }

// === Task doc mic: I2S RX -> sb_mic_pcm ===

void AudioManager::micReadLoop()
{
    ESP_LOGI(TAG, "MicRead task started");
    constexpr size_t PCM_FRAME = 256;
    int16_t pcm_buf[PCM_FRAME];

    while (started) {
        if (!listening || power_saving) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        size_t samples = input->readPcm(pcm_buf, PCM_FRAME);
        if (samples == 0) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        size_t bytes = samples * sizeof(int16_t);
        size_t sent = xStreamBufferSend(sb_mic_pcm, pcm_buf, bytes, pdMS_TO_TICKS(10));
        if (sent < bytes) {
            ESP_LOGW(TAG, "MicRead: buffer full, dropped %zu bytes", bytes - sent);
        }
    }

    ESP_LOGW(TAG, "MicRead task ended");
    vTaskDelete(nullptr);
}

// === Task Stream mic: sb_mic_pcm -> Encode -> sb_mic_encoded ===

void AudioManager::micStreamLoop()
{
    ESP_LOGI(TAG, "MicStream task started");

    const size_t pcm_frame = codec->pcmFrameSamples();
    const size_t max_encoded = codec->encodedFrameBytes() + 16; // Extra for length prefix
    int16_t* pcm_in = new int16_t[pcm_frame];
    uint8_t* encoded = new uint8_t[max_encoded];

    while (started) {
        if (!listening || power_saving) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t pcm_bytes = xStreamBufferReceive(
            sb_mic_pcm, pcm_in, pcm_frame * sizeof(int16_t), pdMS_TO_TICKS(20));

        if (pcm_bytes >= pcm_frame * sizeof(int16_t)) {
            size_t enc_len = codec->encode(pcm_in, pcm_frame, encoded, max_encoded);
            if (enc_len > 0) {
                xStreamBufferSend(sb_mic_encoded, encoded, enc_len, pdMS_TO_TICKS(10));
            }
        }
    }

    delete[] pcm_in;
    delete[] encoded;
    ESP_LOGW(TAG, "MicStream task ended");
    vTaskDelete(nullptr);
}

// === Task Nhan Audio: sb_spk_encoded -> Decode -> sb_spk_pcm ===

void AudioManager::audioRecvLoop()
{
    ESP_LOGI(TAG, "AudioRecv task started");

    const size_t pcm_frame = codec->pcmFrameSamples();
    const size_t max_encoded = codec->encodedFrameBytes() + 16;
    uint8_t* encoded = new uint8_t[max_encoded];
    int16_t* pcm_out = new int16_t[pcm_frame * 4]; // Extra capacity
    bool new_session = true;

    while (started) {
        if (!speaking || power_saving) {
            if (sb_spk_encoded) xStreamBufferReset(sb_spk_encoded);
            if (sb_spk_pcm) xStreamBufferReset(sb_spk_pcm);
            new_session = true;
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t got = xStreamBufferReceive(
            sb_spk_encoded, encoded, max_encoded, pdMS_TO_TICKS(20));

        if (got > 0) {
            if (new_session) {
                codec->reset();
                new_session = false;
                ESP_LOGI(TAG, "New decode session started");
            }

            size_t out_samples = codec->decode(encoded, got, pcm_out, pcm_frame * 4);
            if (out_samples > 0) {
                size_t written = xStreamBufferSend(
                    sb_spk_pcm, pcm_out, out_samples * sizeof(int16_t), pdMS_TO_TICKS(1000));
                if (written != out_samples * sizeof(int16_t)) {
                    ESP_LOGW(TAG, "AudioRecv: SPK buffer full");
                }
            }
        }
    }

    delete[] encoded;
    delete[] pcm_out;
    ESP_LOGW(TAG, "AudioRecv task ended");
    vTaskDelete(nullptr);
}

// === Task phat audio: sb_spk_pcm -> I2S TX ===

void AudioManager::spkPlayLoop()
{
    ESP_LOGI(TAG, "SpkPlay task started");

    constexpr size_t PCM_CHUNK = 1024;
    int16_t pcm_chunk[PCM_CHUNK];
    bool i2s_started = false;

    while (started) {
        if (!speaking || power_saving) {
            if (i2s_started) {
                output->stopPlayback();
                i2s_started = false;
            }
            ulTaskNotifyTake(pdFALSE, pdMS_TO_TICKS(100));
            continue;
        }

        if (!i2s_started) {
            if (!output->startPlayback()) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            i2s_started = true;
            ESP_LOGI(TAG, "I2S playback started");
        }

        size_t got = xStreamBufferReceive(
            sb_spk_pcm, pcm_chunk, sizeof(pcm_chunk), pdMS_TO_TICKS(100));

        if (got >= sizeof(int16_t)) {
            output->writePcm(pcm_chunk, got / sizeof(int16_t));
        }
    }

    if (i2s_started) output->stopPlayback();
    ESP_LOGW(TAG, "SpkPlay task ended");
    vTaskDelete(nullptr);
}
