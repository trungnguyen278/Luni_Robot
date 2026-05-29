#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_sick_fever(GfxEngine& gfx, float t, const ColorContext& colors) {
    float droop = 4.0f + sinf(t * TAU * 0.5f) * 1.5f;
    int16_t y = (int16_t)(CY + droop);
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, y, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 14, 0, colors.eye);

    // Sweat drop
    float sweat = fmodf(t * 1.4f, 1.0f);
    int16_t sx = SCX + 28;
    int16_t sy = (int16_t)(STATUS_H + 18 + sweat * 40.0f);
    uint8_t sop = (uint8_t)((1.0f - sweat * 0.4f) * 255);
    gfx.fillCircle(sx, (int16_t)(sy - 7), 5, colors.accent, sop);
    gfx.fillTriangle((int16_t)(sx - 5), (int16_t)(sy - 7),
                     (int16_t)(sx + 5), (int16_t)(sy - 7),
                     sx, sy, colors.accent, sop);

    // Thermometer bar
    int16_t bx = SCX - 36, by = STATUS_H + 10;
    gfx.strokeRoundedRect(bx, by, 72, 6, 3, colors.eye, 1);
    float fillW = lerp(40.0f, 64.0f, (sinf(t * TAU * 2.0f) + 1.0f) / 2.0f);
    gfx.fillRoundedRect(bx + 2, by + 2, (int16_t)fillW, 2, 1, colors.accent);

    // Downturned mouth
    int16_t mx = SCX, my = SCREEN_H - 30;
    gfx.pushAlpha(178);
    gfx.drawQuadBezier(mx - 15, my, mx, my + 5, mx + 15, my, colors.eye, 4);
    gfx.popAlpha();
}

static void render_sick_queasy(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 1.5f) * 5.0f;
    float ty = cosf(t * TAU * 1.5f) * 3.0f;
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.drawEye((int16_t)(LX + wob), (int16_t)(CY + ty), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX - wob), (int16_t)(CY - ty), EYE_W, h, EYE_RX, 0, colors.eye);

    // Wavy nausea line
    int16_t baseY = SCREEN_H - 28;
    for (int i = 0; i < 8; i++) {
        float x0 = 60.0f + i * 25.0f;
        float x1 = x0 + 25.0f;
        float y0 = (float)baseY + sinf(t * TAU * 2.0f + i * 0.8f) * 6.0f;
        float y1 = (float)baseY + sinf(t * TAU * 2.0f + (i + 1) * 0.8f) * 6.0f;
        gfx.drawLine((int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1,
                     colors.accent, 3);
    }
}

static void render_sick_mask(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breath = sinf(t * TAU) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.7f);
    gfx.drawEye(LX, CY - 8, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY - 8, EYE_W, h, EYE_RX, 0, colors.eye);

    // Surgical mask
    int16_t my = (int16_t)(SCREEN_H - 60 + breath);
    gfx.fillRoundedRect(SCX - 70, my, 140, 42, 20, colors.eye, 217);
    // Fold lines
    gfx.drawLine(SCX - 60, (int16_t)(my + 14), SCX + 60, (int16_t)(my + 14),
                 colors.bg, 1, 128);
    gfx.drawLine(SCX - 60, (int16_t)(my + 24), SCX + 60, (int16_t)(my + 24),
                 colors.bg, 1, 128);
    // Ear loops
    gfx.drawQuadBezier(SCX - 70, (int16_t)(my + 10), (int16_t)(SCX - 96),
                       (int16_t)(my + 20), SCX - 70, (int16_t)(my + 32),
                       colors.eye, 2);
    gfx.drawQuadBezier(SCX + 70, (int16_t)(my + 10), (int16_t)(SCX + 96),
                       (int16_t)(my + 20), SCX + 70, (int16_t)(my + 32),
                       colors.eye, 2);
}

const VariantDef SICK_VARIANTS[] = {
    {"sick-fever",  "Fever",  2800, TONE_NONE, render_sick_fever},
    {"sick-queasy", "Queasy", 2400, TONE_NONE, render_sick_queasy},
    {"sick-mask",   "Mask",   3200, TONE_NONE, render_sick_mask},
};

extern const CategoryDef CAT_SICK = {
    "sick", "Sick", ContentKind::EMOTION, TONE_GREEN,
    SICK_VARIANTS, sizeof(SICK_VARIANTS) / sizeof(SICK_VARIANTS[0])
};
