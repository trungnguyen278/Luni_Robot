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

// Frame size = esp32-camera framesize_t enum value:
//   1=QQVGA(160x120) · 2=QCIF(176x144) · 3=HQVGA(240x176) · 5=QVGA(320x240)
// This board's PSRAM is Octal @ 80MHz so esp32-camera can't DMA frames to PSRAM
// ("PSRAM DMA mode disabled") and falls back to small internal line buffers. At
// 20MHz XCLK anything above QQVGA overran them ("EV-VSYNC-OVF"). We now capture
// for robot/free-space detection (resolution matters, frame rate <=1Hz is fine),
// so we trade speed for size: QVGA + a HALVED XCLK (10MHz, see PinConfig) so the
// fallback has time to drain each line. If EV-VSYNC-OVF / fb_get timeouts return,
// step DOWN this ladder (or drop XCLK to 8MHz): 5 QVGA -> 3 HQVGA -> 2 QCIF -> 1 QQVGA.
static constexpr int FRAME_SIZE          = 5;    // QVGA (320x240)
static constexpr int JPEG_QUALITY        = 12;   // 0..63, lower = better/bigger

// Capture mode. true = OV2640 outputs JPEG directly (hardware encoder): correct
// colors (no RGB565 conversion at all), smaller files, no SW-encode CPU. The old
// "broken encoder / NO-SOI" verdict was actually an XCLK issue (16MHz fails,
// 10/20MHz work — espressif/esp32-camera#657); our XCLK is 10MHz. If NO-SOI
// returns, flip to false to fall back to RGB565 + software JPEG.
static constexpr bool USE_HW_JPEG = true;

// RGB565-fallback color fix (only used when USE_HW_JPEG == false). The right combo
// is board-specific (esp32-camera#422/#676 = byte endianness, #379 = R/B order).
// A byte-swap alone came out magenta, so R/B swap is on too. Flip either + reflash
// if color is still off.
static constexpr bool RGB565_SWAP_BYTES = true;   // high/low byte (endianness)
static constexpr bool RGB565_SWAP_RB    = true;   // red <-> blue (BGR fix)
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
// 85 keeps a QVGA frame well under the C5's 64KB image cap (UartBridge) — raise
// toward 92 for crisper detection if the encoded size stays comfortably below 64KB.
static constexpr int SW_JPEG_QUALITY     = 90;

} // namespace CameraConfig
