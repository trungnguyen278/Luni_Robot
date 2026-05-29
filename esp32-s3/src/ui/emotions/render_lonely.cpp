#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_lonely_sigh(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU - PI / 2.0f);
    float droop = lerp(2.0f, 8.0f, (phase + 1.0f) / 2.0f);
    int16_t y = (int16_t)(CY + droop);
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, y, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 14, 0, colors.eye);

    // Breath-out cloud
    if (phase > 0.2f) {
        uint8_t op = (uint8_t)((phase - 0.2f) * 1.2f * 128);
        float cy2 = SCREEN_H - 24 + (phase - 0.2f) * 14.0f;
        gfx.fillCircle((int16_t)(SCX - 10), (int16_t)cy2, 5, colors.accent, op);
        gfx.fillCircle(SCX, (int16_t)cy2, 6, colors.accent, op);
        gfx.fillCircle((int16_t)(SCX + 10), (int16_t)cy2, 5, colors.accent, op);
    }

    // Isolation ring
    gfx.drawDashedLine(SCX - 120, CY + 8, SCX + 120, CY + 8,
                       colors.accent, 1, 2, 6);
}

static void render_lonely_empty(GfxEngine& gfx, float t, const ColorContext& colors) {
    float driftX = sinf(t * TAU * 0.4f) * 4.0f;
    int16_t w = (int16_t)(EYE_W * 0.9f);
    int16_t h = (int16_t)(EYE_H * 0.6f);

    // Single solo eye centered
    gfx.drawEye((int16_t)(SCX + driftX), CY, w, h, 16, 0, colors.eye);

    // Ghost outlines where eyes "should be"
    gfx.pushAlpha(51);
    gfx.strokeCircle(LX, CY, EYE_W / 3, colors.eye, 1);
    gfx.strokeCircle(RX, CY, EYE_W / 3, colors.eye, 1);
    gfx.popAlpha();

    gfx.drawText("ALONE", SCX, SCREEN_H - 14, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 153);
}

static void render_lonely_window(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t droop = 4;
    int16_t y = CY + droop;

    // Window frame
    int16_t wx = SCX - 70, wy = STATUS_H + 14;
    int16_t ww = 140, wh = SCREEN_H - STATUS_H - 32;
    gfx.pushAlpha(115);
    gfx.strokeRect(wx, wy, ww, wh, colors.accent, 2);
    gfx.drawLine(SCX, wy, SCX, (int16_t)(wy + wh), colors.accent, 2);
    gfx.drawLine(wx, CY, (int16_t)(wx + ww), CY, colors.accent, 2);
    gfx.popAlpha();

    // Eyes looking down
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.fillCircle(LX, (int16_t)(y + 12), 9, colors.bg);
    gfx.fillCircle(RX, (int16_t)(y + 12), 9, colors.bg);

    // Falling rain
    for (int i = 0; i < 6; i++) {
        float seed = fmodf(i * 17.0f, 100.0f) / 100.0f;
        float p = fmodf(t * 1.4f + seed, 1.0f);
        float rx = (float)(SCX - 60) + i * 24.0f + sinf(seed * TAU) * 4.0f;
        float ry = STATUS_H + 16 + p * (SCREEN_H - STATUS_H - 40);
        uint8_t op = (uint8_t)((1.0f - p) * 0.65f * 255);
        gfx.drawLine((int16_t)rx, (int16_t)ry, (int16_t)(rx - 2), (int16_t)(ry + 6),
                     colors.accent, 1, op);
    }
}

const VariantDef LONELY_VARIANTS[] = {
    {"lonely-sigh",   "Lonely sigh",    4400, TONE_NONE, render_lonely_sigh},
    {"lonely-empty",  "Empty room",     5200, TONE_NONE, render_lonely_empty},
    {"lonely-window", "At the window",  4800, TONE_NONE, render_lonely_window},
};

extern const CategoryDef CAT_LONELY = {
    "lonely", "Lonely", ContentKind::EMOTION, TONE_BLUE,
    LONELY_VARIANTS, sizeof(LONELY_VARIANTS) / sizeof(LONELY_VARIANTS[0])
};
