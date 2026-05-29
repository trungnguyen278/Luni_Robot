#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_calm_breathe(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = (sinf(t * TAU - PI / 2.0f) + 1.0f) / 2.0f;
    float depth = lerp(10.0f, 22.0f, phase);
    int16_t sw = (int16_t)lerp(10.0f, 13.0f, phase);
    int16_t hw = (EYE_W - 8) / 2;

    gfx.drawQuadBezier(LX - hw, CY, LX, (int16_t)(CY + depth), LX + hw, CY,
                       colors.eye, sw);
    gfx.drawQuadBezier(RX - hw, CY, RX, (int16_t)(CY + depth), RX + hw, CY,
                       colors.eye, sw);

    // Breathing dot
    int16_t r = (int16_t)lerp(3.0f, 6.0f, phase);
    gfx.fillCircle(SCX, SCREEN_H - 28, r, colors.accent, 178);
}

static void render_calm_zen(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Slowly-drawing enso circle
    float drawn = clamp(t * 1.3f, 0.0f, 1.0f);
    float startA = PI * 0.75f;
    float endA = startA + drawn * PI * 1.7f;
    gfx.drawArc(SCX, CY, 60, startA, endA, colors.accent, 6, 140);

    // Closed eyes
    int16_t hw = (EYE_W - 16) / 2;
    gfx.drawQuadBezier(LX - hw, CY + 4, LX, CY + 4 + 14, LX + hw, CY + 4,
                       colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, CY + 4, RX, CY + 4 + 14, RX + hw, CY + 4,
                       colors.eye, 10);
}

static void render_calm_lotus(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breath = (sinf(t * TAU - PI / 2.0f) + 1.0f) / 2.0f;
    int16_t hw = (EYE_W - 14) / 2;
    float depth = lerp(12.0f, 18.0f, breath);

    gfx.drawQuadBezier(LX - hw, CY, LX, (int16_t)(CY + depth), LX + hw, CY,
                       colors.eye, 11);
    gfx.drawQuadBezier(RX - hw, CY, RX, (int16_t)(CY + depth), RX + hw, CY,
                       colors.eye, 11);

    // Lotus petals
    float degs[] = {-30.0f, -15.0f, 0.0f, 15.0f, 30.0f};
    for (int i = 0; i < 5; i++) {
        float a = degs[i] * PI / 180.0f;
        float px = (float)SCX + sinf(a) * 36.0f;
        float py = (float)(SCREEN_H - 36) + (1.0f - cosf(a)) * -10.0f;
        int16_t rx = (int16_t)(5.0f + breath * 1.5f);
        int16_t ry = (int16_t)(11.0f + breath * 2.0f);
        uint8_t op = (uint8_t)((0.5f + i * 0.08f) * 178);
        gfx.pushTransform();
        gfx.translate(px, py);
        gfx.rotate(degs[i], 0, 0);
        gfx.fillEllipse(0, 0, rx, ry, colors.accent, op);
        gfx.popTransform();
    }
}

const VariantDef CALM_VARIANTS[] = {
    {"calm-breathe", "Slow breathe", 6000, TONE_NONE, render_calm_breathe},
    {"calm-zen",     "Zen circle",   4800, TONE_NONE, render_calm_zen},
    {"calm-lotus",   "Lotus",        4000, TONE_NONE, render_calm_lotus},
};

extern const CategoryDef CAT_CALM = {
    "calm", "Calm", ContentKind::EMOTION, TONE_CYAN,
    CALM_VARIANTS, sizeof(CALM_VARIANTS) / sizeof(CALM_VARIANTS[0])
};
