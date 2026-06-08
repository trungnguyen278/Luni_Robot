#include "CameraManager.hpp"
#include "esp_log.h"

static const char* TAG = "CameraManager";

#ifdef LUNI_ENABLE_CAMERA
#include "esp_camera.h"
#include "config/PinConfig.hpp"
#include "config/CameraConfig.hpp"

bool CameraManager::init()
{
    if (ready_) return true;

    camera_config_t cfg = {};
    cfg.pin_pwdn     = PinConfig::Camera::PWDN;
    cfg.pin_reset    = PinConfig::Camera::RESET;
    cfg.pin_xclk     = PinConfig::Camera::XCLK;
    cfg.pin_sccb_sda = PinConfig::Camera::SIOD;
    cfg.pin_sccb_scl = PinConfig::Camera::SIOC;
    cfg.pin_d7       = PinConfig::Camera::D7;
    cfg.pin_d6       = PinConfig::Camera::D6;
    cfg.pin_d5       = PinConfig::Camera::D5;
    cfg.pin_d4       = PinConfig::Camera::D4;
    cfg.pin_d3       = PinConfig::Camera::D3;
    cfg.pin_d2       = PinConfig::Camera::D2;
    cfg.pin_d1       = PinConfig::Camera::D1;
    cfg.pin_d0       = PinConfig::Camera::D0;
    cfg.pin_vsync    = PinConfig::Camera::VSYNC;
    cfg.pin_href     = PinConfig::Camera::HREF;
    cfg.pin_pclk     = PinConfig::Camera::PCLK;
    cfg.xclk_freq_hz = PinConfig::Camera::XCLK_FREQ_HZ;

    // Use LEDC TIMER_1 / CHANNEL_1 — TIMER_0 / CHANNEL_0 belong to the LCD
    // backlight (see DisplayDriver). Avoid the collision.
    cfg.ledc_timer   = LEDC_TIMER_1;
    cfg.ledc_channel = LEDC_CHANNEL_1;

    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size   = (framesize_t)CameraConfig::FRAME_SIZE;
    cfg.jpeg_quality = CameraConfig::JPEG_QUALITY;
    cfg.fb_count     = CameraConfig::FB_COUNT;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return false;
    }
    ready_ = true;
    ESP_LOGI(TAG, "Camera ready (QVGA JPEG)");
    return true;
}

void CameraManager::deinit()
{
    if (!ready_) return;
    releaseFrame();
    esp_camera_deinit();
    ready_ = false;
}

bool CameraManager::captureJpeg(const uint8_t** out, size_t* out_len,
                                uint16_t* w, uint16_t* h)
{
    if (out) *out = nullptr;
    if (out_len) *out_len = 0;
    if (w) *w = 0;
    if (h) *h = 0;
    if (!ready_) return false;

    releaseFrame();   // drop any previously held frame
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGW(TAG, "fb_get returned null");
        return false;
    }
    fb_ = fb;
    if (out) *out = fb->buf;
    if (out_len) *out_len = fb->len;
    if (w) *w = (uint16_t)fb->width;
    if (h) *h = (uint16_t)fb->height;
    return true;
}

void CameraManager::releaseFrame()
{
    if (fb_) {
        esp_camera_fb_return((camera_fb_t*)fb_);
        fb_ = nullptr;
    }
}

#else  // LUNI_ENABLE_CAMERA not defined — compile no-op stubs.

bool CameraManager::init()
{
    ESP_LOGW(TAG, "camera disabled (define LUNI_ENABLE_CAMERA to enable)");
    return false;
}
void CameraManager::deinit() {}
bool CameraManager::captureJpeg(const uint8_t** out, size_t* out_len,
                                uint16_t* w, uint16_t* h)
{
    if (out) *out = nullptr;
    if (out_len) *out_len = 0;
    if (w) *w = 0;
    if (h) *h = 0;
    return false;
}
void CameraManager::releaseFrame() {}

#endif  // LUNI_ENABLE_CAMERA
