#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// error-bang: wide eyes + exclamation mark (!) above blinking
static void render_error_bang(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 6.0f) * 2.0f;
    int16_t w = (int16_t)(EYE_W * 1.15f);
    int16_t h = (int16_t)(EYE_H * 1.15f);
    gfx.drawEye((int16_t)(LX + shake), CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX - shake), CY, w, h, EYE_RX, 0, colors.eye);

    // Exclamation mark blinking
    float blink_val = (sinf(t * TAU * 2.5f) + 1.0f) / 2.0f;
    uint8_t bangAlpha = (uint8_t)(80 + blink_val * 175.0f);

    // "!" stem
    int16_t bx = SCX;
    int16_t by = STATUS_H + 10;
    gfx.fillRoundedRect(bx - 3, by, 6, 22, 3, colors.accent, bangAlpha);
    // "!" dot
    gfx.fillCircle(bx, by + 30, 4, colors.accent, bangAlpha);
}

// error-noconn: normal eyes + "wifi icon with X" shape below, blinking
static void render_error_noconn(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.75f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Wifi icon: concentric arcs below eyes
    float blink_val = (sinf(t * TAU * 1.5f) + 1.0f) / 2.0f;
    uint8_t wifiAlpha = (uint8_t)(100 + blink_val * 155.0f);
    int16_t wx = SCX;
    int16_t wy = SCREEN_H - 30;

    // Three arcs of decreasing radius (wifi-like)
    for (int i = 0; i < 3; i++) {
        int16_t r = (int16_t)(28 - i * 8);
        float start = -PI * 0.75f;
        float end = -PI * 0.25f;
        gfx.drawArc(wx, wy, r, start, end, colors.accent, 3, wifiAlpha);
    }
    // Center dot
    gfx.fillCircle(wx, (int16_t)(wy - 2), 3, colors.accent, wifiAlpha);

    // X over the wifi icon
    int16_t xsz = 14;
    gfx.drawLine(wx - xsz, wy - xsz - 6, wx + xsz, wy + xsz - 6,
                 colors.eye, 3, wifiAlpha);
    gfx.drawLine(wx + xsz, wy - xsz - 6, wx - xsz, wy + xsz - 6,
                 colors.eye, 3, wifiAlpha);
}

// error-pixels: eyes flicker with random block glitches
static void render_error_pixels(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Base eyes with occasional flicker
    float flicker = sinf(t * TAU * 12.0f);
    uint8_t eyeAlpha = (flicker > 0.7f) ? 140 : 255;
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye, eyeAlpha);
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye, eyeAlpha);

    // Glitch blocks: scattered rectangles that appear/disappear
    // Use deterministic pseudo-random based on time
    int frame = (int)(t * 15.0f);
    for (int i = 0; i < 5; i++) {
        int seed = (frame + i * 37) % 100;
        if (seed > 55) continue; // Only draw some blocks

        // Deterministic positions from seed
        int16_t gx = (int16_t)(20 + ((seed * 67 + i * 41) % (SCREEN_W - 40)));
        int16_t gy = (int16_t)(STATUS_H + 10 + ((seed * 31 + i * 53) % (SCREEN_H - STATUS_H - 30)));
        int16_t gw = (int16_t)(6 + (seed % 16));
        int16_t gh = (int16_t)(3 + (seed % 8));

        uint8_t gop = (uint8_t)(100 + (seed % 155));
        uint16_t gcol = (i % 2 == 0) ? colors.accent : colors.eye;
        gfx.fillRect(gx, gy, gw, gh, gcol, gop);
    }
}

// error-warning: wide eyes + triangle warning shape with ! inside, pulsing
static void render_error_warning(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pulse_val = sinf(t * TAU * 2.0f);
    float shake = pulse_val * 1.5f;
    int16_t w = (int16_t)(EYE_W * 1.1f);
    int16_t h = (int16_t)(EYE_H * 1.05f);
    gfx.drawEye((int16_t)(LX + shake), CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX - shake), CY, w, h, EYE_RX, 0, colors.eye);

    // Warning triangle above center
    float scale = 1.0f + (pulse_val + 1.0f) / 2.0f * 0.08f;
    uint8_t triAlpha = (uint8_t)(150 + ((pulse_val + 1.0f) / 2.0f) * 105.0f);

    int16_t tx = SCX;
    int16_t ty = STATUS_H + 8;
    int16_t tsz = (int16_t)(22 * scale);

    // Triangle outline (pointing up)
    int16_t x0 = tx;
    int16_t y0 = ty;
    int16_t x1 = (int16_t)(tx - tsz);
    int16_t y1 = (int16_t)(ty + tsz * 1.6f);
    int16_t x2 = (int16_t)(tx + tsz);
    int16_t y2 = y1;

    gfx.drawLine(x0, y0, x1, y1, colors.accent, 3, triAlpha);
    gfx.drawLine(x1, y1, x2, y2, colors.accent, 3, triAlpha);
    gfx.drawLine(x2, y2, x0, y0, colors.accent, 3, triAlpha);

    // "!" inside triangle
    int16_t bangY = (int16_t)(ty + tsz * 0.5f);
    gfx.fillRoundedRect(tx - 2, bangY, 4, (int16_t)(tsz * 0.5f), 2,
                        colors.accent, triAlpha);
    gfx.fillCircle(tx, (int16_t)(bangY + tsz * 0.7f), 3, colors.accent, triAlpha);
}

// --- Category registration ---
const VariantDef ERROR_VARIANTS[] = {
    {"error-bang",    "Bang",    2400, TONE_NONE, render_error_bang},
    {"error-noconn",  "No conn", 2800, TONE_NONE, render_error_noconn},
    {"error-pixels",  "Pixels",  3000, TONE_NONE, render_error_pixels},
    {"error-warning", "Warning", 2600, TONE_NONE, render_error_warning},
};

extern const CategoryDef CAT_ERROR = {
    "error", "Error", ContentKind::EMOTION, TONE_RED,
    ERROR_VARIANTS, sizeof(ERROR_VARIANTS) / sizeof(ERROR_VARIANTS[0])
};
