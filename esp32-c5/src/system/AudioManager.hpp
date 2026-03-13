#pragma once
#include <memory>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "system/StateTypes.hpp"
#include "system/StateManager.hpp"

class AudioInput;
class AudioOutput;
class AudioCodec;

// Audio pipeline manager for ESP32-C5.
// Tasks (matching architecture diagram):
//   - Task doc mic:     I2S RX -> Buffer MIC DMA (sb_mic_pcm)
//   - Task Stream mic:  sb_mic_pcm -> Encode -> sb_mic_encoded (for uplink)
//   - Task Nhan Audio:  sb_spk_encoded (downlink) -> Decode -> sb_spk_pcm
//   - Task phat audio:  sb_spk_pcm -> I2S TX
class AudioManager {
public:
    AudioManager();
    ~AudioManager();

    // Lifecycle
    bool init();
    void start();
    void stop();
    bool allocateResources();
    void freeResources();

    // Dependency injection
    void setInput(std::unique_ptr<AudioInput> in);
    void setOutput(std::unique_ptr<AudioOutput> out);
    void setCodec(std::unique_ptr<AudioCodec> cdc);

    // Stream buffer access (for NetworkManager uplink/downlink)
    StreamBufferHandle_t getMicEncodedBuffer()     const { return sb_mic_encoded; }
    StreamBufferHandle_t getSpeakerEncodedBuffer() const { return sb_spk_encoded; }

    // Control
    void setPowerSaving(bool enable);
    void setVolume(uint8_t percent);

    // Audio actions
    void startListening(state::InputSource src);
    void pauseListening();
    void stopAll();
    void stopListening();
    void startSpeaking();
    void stopSpeaking();

private:
    void handleInteractionState(state::InteractionState s, state::InputSource src);

    // Task entries
    static void micReadTaskEntry(void* arg);
    static void micStreamTaskEntry(void* arg);
    static void audioRecvTaskEntry(void* arg);
    static void spkPlayTaskEntry(void* arg);

    // Task loops
    void micReadLoop();      // Task doc mic
    void micStreamLoop();    // Task Stream mic (encode)
    void audioRecvLoop();    // Task Nhan Audio (decode)
    void spkPlayLoop();      // Task phat audio

    // State
    std::atomic<bool> started{false};
    std::atomic<bool> listening{false};
    std::atomic<bool> speaking{false};
    std::atomic<bool> power_saving{false};
    state::InputSource current_source = state::InputSource::UNKNOWN;

    // Components
    std::unique_ptr<AudioInput>  input;
    std::unique_ptr<AudioOutput> output;
    std::unique_ptr<AudioCodec>  codec;

    // Stream buffers
    StreamBufferHandle_t sb_mic_pcm     = nullptr; // Raw PCM from mic (Buffer MIC DMA)
    StreamBufferHandle_t sb_mic_encoded = nullptr; // Encoded uplink
    StreamBufferHandle_t sb_spk_pcm     = nullptr; // Decoded PCM to speaker (Buffer Speaker)
    StreamBufferHandle_t sb_spk_encoded = nullptr; // Encoded downlink

    // Tasks
    TaskHandle_t mic_read_task   = nullptr;
    TaskHandle_t mic_stream_task = nullptr;
    TaskHandle_t audio_recv_task = nullptr;
    TaskHandle_t spk_play_task   = nullptr;

    int sub_interaction_id = -1;
};
