#include "CameraManager.hpp"
#include "esp_log.h"

static const char* TAG = "CameraManager";

#ifdef LUNI_ENABLE_CAMERA
#include "esp_camera.h"
#include "img_converters.h"   // frame2jpg — software RGB565 -> JPEG encode
#include "esp_heap_caps.h"
#include <cstdlib>            // free()
#include "config/PinConfig.hpp"
#include "config/CameraConfig.hpp"

// Fill the board-fixed pins + clock. Pixel format / frame size / fb settings are
// left to the caller.
static void applyBoardConfig(camera_config_t& cfg)
{
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
}

bool CameraManager::init()
{
    if (ready_) return true;

    camera_config_t cfg = {};
    applyBoardConfig(cfg);
    // Two capture modes (CameraConfig::USE_HW_JPEG):
    //  - HW JPEG: the OV2640 outputs JPEG directly → correct colors, small files,
    //    no SW encode. Preferred. (NO-SOI was an XCLK issue, not a dead encoder.)
    //  - RGB565: raw frames + software frame2jpg in captureJpeg() (fallback).
    cfg.pixel_format = CameraConfig::USE_HW_JPEG ? PIXFORMAT_JPEG : PIXFORMAT_RGB565;
    cfg.frame_size   = (framesize_t)CameraConfig::FRAME_SIZE;
    cfg.jpeg_quality = CameraConfig::JPEG_QUALITY;   // HW-JPEG only (0..63)
    cfg.fb_count     = CameraConfig::FB_COUNT;
    cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: %s", esp_err_to_name(err));
        return false;
    }
    ready_ = true;

    // Make sure auto-exposure / gain / white-balance are on and bias a bit
    // brighter — captured frames were coming out near-black (JPEG only ~2KB).
    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_exposure_ctrl(s, 1);                 // AEC on
        s->set_aec2(s, 1);                          // low-light AEC algorithm
        s->set_gain_ctrl(s, 1);                     // AGC on
        s->set_gainceiling(s, GAINCEILING_16X);     // allow more gain in dim light
        s->set_whitebal(s, 1);                      // AWB on
        s->set_awb_gain(s, 1);
        s->set_brightness(s, 1);                    // -2..2, nudge brighter
    }

    // Warm-up: the OV2640 emits dark/half-exposed frames right after init while
    // AEC/AGC converge. Pull and drop several so the first real capture is clean.
    for (int i = 0; i < 8; ++i) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (fb) esp_camera_fb_return(fb);
    }

    ESP_LOGI(TAG, "Camera ready (%s) PSRAM free=%u",
             CameraConfig::USE_HW_JPEG ? "HW JPEG" : "RGB565 -> SW JPEG",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
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

    releaseFrame();   // free any previously encoded JPEG

    // Drop a few frames so AEC/AGC adjust to the current scene before we keep one
    // (captures are on-demand and infrequent — the first frame is often stale/dark).
    camera_fb_t* fb = nullptr;
    for (int i = 0; i < 4; ++i) {
        if (fb) esp_camera_fb_return(fb);
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "fb_get returned null");
            return false;
        }
    }

    const uint16_t fw = (uint16_t)fb->width;
    const uint16_t fh = (uint16_t)fb->height;

    if (CameraConfig::USE_HW_JPEG) {
        // Sensor already produced a JPEG — no color conversion, correct colors.
        // The OV2640 occasionally emits a frame with no SOI (0xFFD8) marker; reject
        // and retry a few frames rather than ship a corrupt JPEG. Retain the fb
        // (zero-copy) and hand its buffer out; releaseFrame() returns it.
        int tries = 0;
        while (fb && !(fb->len >= 2 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8)) {
            ESP_LOGW(TAG, "JPEG NO-SOI (len=%u), retry %d", (unsigned)fb->len, tries);
            esp_camera_fb_return(fb);
            if (++tries > 5) { fb = nullptr; break; }
            fb = esp_camera_fb_get();
        }
        if (!fb) {
            ESP_LOGW(TAG, "HW JPEG capture failed (NO-SOI)");
            return false;
        }
        fb_ = fb;                  // owned until releaseFrame()
        jpg_ = fb->buf;
        jpg_len_ = fb->len;
        ESP_LOGI(TAG, "captured HW-JPEG %u bytes (%ux%u)", (unsigned)fb->len, fw, fh);
    } else {
        // Raw RGB565 → software JPEG. Fix color in place before encoding: the
        // OV2640/S3 byte order and R/B order don't match frame2jpg's RGB565 reader
        // (esp32-camera#422/#676 endianness, #379 R/B). Both fixes are config-gated
        // so the right combo can be found empirically. Buffer is ours until return.
        uint8_t* px = fb->buf;
        for (size_t i = 0; i + 1 < fb->len; i += 2) {
            uint16_t v = (uint16_t)(px[i] | (px[i + 1] << 8));   // native (assume LE)
            if (CameraConfig::RGB565_SWAP_RB) {
                const uint16_t r = (v >> 11) & 0x1F;
                const uint16_t b = v & 0x1F;
                v = (uint16_t)((b << 11) | (v & 0x07E0) | r);    // swap 5-bit R/B
            }
            if (CameraConfig::RGB565_SWAP_BYTES) {               // emit big-endian
                px[i]     = (uint8_t)(v >> 8);
                px[i + 1] = (uint8_t)(v & 0xFF);
            } else {
                px[i]     = (uint8_t)(v & 0xFF);
                px[i + 1] = (uint8_t)(v >> 8);
            }
        }

        uint8_t* jpg = nullptr;
        size_t   jlen = 0;
        const bool ok = frame2jpg(fb, CameraConfig::SW_JPEG_QUALITY, &jpg, &jlen);
        esp_camera_fb_return(fb);

        if (!ok || !jpg || jlen < 2) {
            ESP_LOGW(TAG, "frame2jpg failed (ok=%d len=%u)", ok ? 1 : 0, (unsigned)jlen);
            if (jpg) free(jpg);
            return false;
        }
        jpg_ = jpg;
        jpg_len_ = jlen;
        ESP_LOGI(TAG, "captured SW-JPEG %u bytes (%ux%u)", (unsigned)jlen, fw, fh);
    }

    if (out) *out = jpg_;
    if (out_len) *out_len = jpg_len_;
    if (w) *w = fw;
    if (h) *h = fh;
    return true;
}

void CameraManager::releaseFrame()
{
    if (fb_) {
        // HW-JPEG: jpg_ aliased fb_->buf — give the frame buffer back, don't free.
        esp_camera_fb_return((camera_fb_t*)fb_);
        fb_ = nullptr;
    } else if (jpg_) {
        // RGB565: jpg_ is our frame2jpg malloc.
        free(jpg_);
    }
    jpg_ = nullptr;
    jpg_len_ = 0;
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
