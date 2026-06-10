#pragma once
#include <cstdint>

// =============================================================================
// Camera tunables (Freenove ESP32-S3-WROOM CAM / OV2640). Pins live in
// PinConfig::Camera. FRAME_SIZE uses the esp32-camera framesize_t numeric value
// to avoid pulling esp_camera.h into a config header. EXACT enum values:
//   0=96X96 · 1=QQVGA(160x120) · 2=QCIF(176x144) · 3=HQVGA(240x176) ·
//   4=240X240 · 5=QVGA(320x240) · 8=VGA(640x480)
// Keep it small for fast UART transfer (image goes S3→UART→C5→WS→server).
// =============================================================================
namespace CameraConfig {

// QQVGA (160x120) = enum value 1 (NOT 2 — that's QCIF). This board's PSRAM is
// Octal @ 80MHz so esp32-camera can't DMA frames to PSRAM ("PSRAM DMA mode
// disabled") and falls back to small internal line buffers. At anything above
// QQVGA those can't drain a frame before the next VSYNC -> "EV-VSYNC-OVF" +
// timeout. QQVGA's lower data rate fits the fallback path (verified: a clean
// RGB565 QQVGA frame captured at 20MHz XCLK with fb_count=1). Raise resolution
// only if camera-to-PSRAM DMA gets re-enabled (needs a Quad-PSRAM board).
static constexpr int FRAME_SIZE          = 1;    // QQVGA (160x120)
static constexpr int JPEG_QUALITY        = 12;   // 0..63, lower = better/bigger
// MUST be 1, not 2. With the "PSRAM DMA mode disabled" fallback, fb_count=2 makes
// esp32-camera's DMA sizing allocate a frame buffer SMALLER than one frame (saw
// 32768 < 38400 for QQVGA RGB565) -> it can't hold a full frame -> EV-VSYNC-OVF +
// timeout. fb_count=1 sizes it correctly (38400) and captures cleanly — exactly
// the config the working RGB565 probe used. On-demand stills don't need 2 anyway.
static constexpr int FB_COUNT            = 1;
static constexpr int CAPTURE_INTERVAL_MS = 0;    // 0 = on-demand only (no periodic)

// This OV2640 module's HARDWARE JPEG encoder is broken — it returns frames with
// no SOI marker ("NO-SOI - JPEG start marker missing") while raw RGB565 capture
// is clean (verified). So we grab RGB565 and JPEG-encode in software on the S3
// (frame2jpg) — see CameraManager. This quality is the frame2jpg scale: 0..100,
// HIGHER = better/bigger (NOT the esp_camera 0..63 scale of JPEG_QUALITY above).
static constexpr int SW_JPEG_QUALITY     = 80;

} // namespace CameraConfig
