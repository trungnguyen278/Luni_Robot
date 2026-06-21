#pragma once
#include <cstdint>
#include <cstddef>

// =============================================================================
// Luni audio tunables — extracted here so sample rate / codec params can be
// changed in one place without touching the pipeline code.
//
// Moved to 16 kHz + Opus VOIP: lighter on Core 1 (frees CPU for wake word /
// vision) and native 16 kHz for the wake-word frontend (no downsampling).
// NOTE: the server must use the SAME sample rate / Opus mode to decode.
// =============================================================================
namespace AudioConfig {

// --- Sampling ---
static constexpr uint32_t SAMPLE_RATE   = 16000;   // mic + speaker + Opus
static constexpr int      FRAME_MS      = 60;      // Opus frame duration (xiaozhi uses 60ms: 3x fewer packets through SPI/queues, better SILK efficiency)
static constexpr size_t   FRAME_SAMPLES = SAMPLE_RATE * FRAME_MS / 1000; // 960
static constexpr uint8_t  CHANNELS      = 1;

// --- Mic gain ---
// ICS43434 is 24-bit left-aligned in a 32-bit slot. Shift selects the gain:
// >>16 = 0dB (too quiet for far-field speech), >>14 = +12dB, >>12 = +24dB
// (xiaozhi uses >>12). Samples are saturated to int16 after the shift.
static constexpr int      MIC_GAIN_SHIFT = 14;

// --- Opus codec ---
static constexpr bool     OPUS_USE_VOIP    = true;   // VOIP(SILK) for speech vs AUDIO(CELT)
static constexpr int      OPUS_BITRATE     = 24000;  // bps — good speech at 16k
static constexpr int      OPUS_COMPLEXITY  = 3;      // 0..10 (higher = more CPU)
static constexpr bool     OPUS_VBR         = true;
// Constrained VBR bounds frame size near bitrate*duration (~180B @24kbps/60ms)
// so every [2B len][opus] frame fits in one SPI payload (MAX_PAYLOAD=250).
static constexpr bool     OPUS_VBR_CONSTRAINED = true;
static constexpr size_t   MAX_ENCODED_BYTES = 240;   // max Opus frame bytes; must stay <= SPI MAX_PAYLOAD-2

// --- I2S DMA (full-duplex) ---
// 6 x 240 frames @16k = ~90ms buffer. Lower DMA_FRAME_NUM for less latency.
static constexpr int      DMA_DESC_NUM  = 6;
static constexpr int      DMA_FRAME_NUM = 240;

} // namespace AudioConfig
