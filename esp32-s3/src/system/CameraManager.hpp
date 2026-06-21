#pragma once
#include <cstdint>
#include <cstddef>

// =============================================================================
// CameraManager — optional OV2640 wrapper for the Freenove ESP32-S3-WROOM CAM.
//
// DISABLED BY DEFAULT to keep the core build independent of the (heavy)
// esp32-camera component. To enable:
//   1) add to esp32-s3/src/idf_component.yml:   espressif/esp32-camera: "^2.0.0"
//   2) add to platformio.ini build_flags:       -DLUNI_ENABLE_CAMERA
//
// Camera pins come from PinConfig::Camera (board-fixed). XCLK uses LEDC TIMER_1
// / CHANNEL_1 so it does NOT collide with the LCD backlight (TIMER_0/CHANNEL_0).
// Init is lazy (call when the camera feature is actually needed) and needs PSRAM
// for the frame buffer (available: 8MB).
// =============================================================================
class CameraManager
{
public:
    bool init();
    void deinit();
    bool isReady() const { return ready_; }

    // Capture one JPEG frame. On success *out / *out_len point into a buffer
    // owned by the driver — call releaseFrame() when done. Optional w/h receive
    // the frame dimensions. Returns false when disabled or capture fails.
    bool captureJpeg(const uint8_t** out, size_t* out_len,
                     uint16_t* w = nullptr, uint16_t* h = nullptr);
    void releaseFrame();

private:
    bool      ready_   = false;
    // Points to the JPEG bytes returned by captureJpeg(). In HW-JPEG mode it
    // aliases fb_->buf (zero-copy, no free); in RGB565 mode it's a frame2jpg
    // malloc we own (freed in releaseFrame).
    uint8_t*  jpg_     = nullptr;
    size_t    jpg_len_ = 0;
    // Retained camera frame buffer for HW-JPEG mode (returned to the driver in
    // releaseFrame). Stored as void* so the header stays free of esp_camera.h.
    // nullptr in RGB565 mode. (camera_fb_t* under the hood.)
    void*     fb_      = nullptr;
};
