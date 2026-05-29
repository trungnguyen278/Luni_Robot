#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Shared: draw default mute eyes (normal with slight droop)
static void drawMuteEyes(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.5f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.85f);
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
}

// mute-x: normal eyes + X mark drawn over mouth area (two crossing lines)
static void render_mute_x(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawMuteEyes(gfx, t, colors);

    // X mark at mouth area
    float pulse_val = (sinf(t * TAU * 1.5f) + 1.0f) / 2.0f;
    uint8_t xAlpha = (uint8_t)(160 + pulse_val * 95.0f);
    int16_t mx = SCX;
    int16_t my = SCREEN_H - 34;
    int16_t sz = 12;

    gfx.drawLine(mx - sz, my - sz, mx + sz, my + sz, colors.accent, 4, xAlpha);
    gfx.drawLine(mx + sz, my - sz, mx - sz, my + sz, colors.accent, 4, xAlpha);
}

// mute-zip: normal eyes + zipper line across bottom (horizontal dashed line)
static void render_mute_zip(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawMuteEyes(gfx, t, colors);

    // Zipper: horizontal dashed line across bottom
    int16_t zy = SCREEN_H - 34;
    int16_t zx0 = SCX - 50;
    int16_t zx1 = SCX + 50;

    gfx.drawDashedLine(zx0, zy, zx1, zy, colors.accent, 3, 8, 5);

    // Small vertical teeth on the dashes
    float anim = fmodf(t * 0.5f, 1.0f);
    int teeth = 8;
    for (int i = 0; i < teeth; i++) {
        float frac = (float)i / (float)(teeth - 1);
        int16_t tx = (int16_t)(zx0 + frac * (zx1 - zx0));
        int16_t th = (int16_t)(3 + sinf(t * TAU * 2.0f + frac * PI) * 2.0f);
        gfx.drawLine(tx, (int16_t)(zy - th), tx, (int16_t)(zy + th),
                     colors.accent, 1, 180);
    }
}

// mute-hush: narrow eyes + finger-to-lips shape (vertical line near center)
static void render_mute_hush(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.4f) * 1.0f;
    int16_t h = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Finger-to-lips: vertical line with small circle on top
    float sway = sinf(t * TAU * 0.6f) * 2.0f;
    int16_t fx = (int16_t)(SCX + sway);
    int16_t fy = SCREEN_H - 46;
    int16_t fLen = 22;

    // Finger line
    gfx.fillRoundedRect(fx - 3, fy, 6, fLen, 3, colors.accent, 200);
    // Fingertip circle
    gfx.fillCircle(fx, (int16_t)(fy - 2), 5, colors.accent, 200);

    // Small "shh" lines radiating out
    for (int i = 0; i < 2; i++) {
        float p = fmodf(t * 1.2f + i * 0.5f, 1.0f);
        float dist = 10.0f + p * 16.0f;
        uint8_t op = (uint8_t)((1.0f - p) * 140.0f);
        int16_t side = (i == 0) ? -1 : 1;
        gfx.drawLine((int16_t)(fx + side * dist), (int16_t)(fy + 4),
                     (int16_t)(fx + side * (dist + 6)), (int16_t)(fy - 2),
                     colors.accent, 2, op);
    }
}

// mute-line: normal eyes + horizontal strike-through line across mouth area
static void render_mute_line(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawMuteEyes(gfx, t, colors);

    // Horizontal line across mouth area
    float pulse_val = (sinf(t * TAU * 1.0f) + 1.0f) / 2.0f;
    uint8_t lineAlpha = (uint8_t)(170 + pulse_val * 85.0f);
    int16_t my = SCREEN_H - 34;

    gfx.drawLine(SCX - 40, my, SCX + 40, my, colors.accent, 4, lineAlpha);

    // Small end caps
    gfx.drawLine(SCX - 40, my - 4, SCX - 40, my + 4, colors.accent, 2, lineAlpha);
    gfx.drawLine(SCX + 40, my - 4, SCX + 40, my + 4, colors.accent, 2, lineAlpha);
}

// --- Category registration ---
const VariantDef MUTE_VARIANTS[] = {
    {"mute-x",    "X",    2400, TONE_NONE, render_mute_x},
    {"mute-zip",  "Zip",  2600, TONE_NONE, render_mute_zip},
    {"mute-hush", "Hush", 2800, TONE_NONE, render_mute_hush},
    {"mute-line", "Line", 2400, TONE_NONE, render_mute_line},
};

extern const CategoryDef CAT_MUTE = {
    "mute", "Mute", ContentKind::EMOTION, TONE_CYAN,
    MUTE_VARIANTS, sizeof(MUTE_VARIANTS) / sizeof(MUTE_VARIANTS[0])
};
