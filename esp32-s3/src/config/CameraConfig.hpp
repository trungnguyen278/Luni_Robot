#pragma once
#include <cstdint>

// =============================================================================
// Camera tunables (Freenove ESP32-S3-WROOM CAM / OV2640). Pins live in
// PinConfig::Camera. FRAME_SIZE uses the esp32-camera framesize_t numeric value
// to avoid pulling esp_camera.h into a config header.
//   QQVGA=2 (160x120) · QVGA=5 (320x240) · VGA=8 (640x480)
// Keep it small for fast UART transfer (image goes S3→UART→C5→WS→server).
// =============================================================================
namespace CameraConfig {

static constexpr int FRAME_SIZE          = 5;    // QVGA (320x240)
static constexpr int JPEG_QUALITY        = 12;   // 0..63, lower = better/bigger
static constexpr int FB_COUNT            = 2;
static constexpr int CAPTURE_INTERVAL_MS = 0;    // 0 = on-demand only (no periodic)

} // namespace CameraConfig
