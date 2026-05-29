#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_greet_bow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU - PI / 2.0f);
    float dy = phase > 0 ? phase * 18.0f : 0;
    float close = phase > 0 ? phase * 0.7f : 0;
    int16_t h = (int16_t)lerp((float)EYE_H, 8.0f, close);
    int16_t y = (int16_t)(CY + dy);
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
}

static void render_greet_wave(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.95f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    float tilt = sinf(t * TAU * 2.0f) * 6.0f;
    gfx.pushTransform();
    gfx.translate((float)(SCREEN_W - 30), (float)(STATUS_H + 22));
    gfx.rotate(tilt, 0, 0);
    gfx.drawLine(0, -16, 0, 6, colors.accent, 4);
    gfx.drawLine(-8, 0, 8, 0, colors.accent, 4);
    gfx.popTransform();
}

static void render_greet_nod(GfxEngine& gfx, float t, const ColorContext& colors) {
    float nod1 = pulse(t, 0.3f, 0.12f);
    float nod2 = pulse(t, 0.55f, 0.12f);
    float dy = (nod1 + nod2) * 10.0f;
    float b = blink(t, 0.3f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
}

static void render_greet_sparkle(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 1.05f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    for (int i = 0; i < 4; i++) {
        float p = fmodf(t + i * 0.25f, 1.0f);
        float x = 40.0f + i * 80.0f;
        float y = lerp((float)(SCREEN_H - 20), (float)(STATUS_H + 10), p);
        float s = lerp(6.0f, 2.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 255);
        gfx.drawLine((int16_t)(x - s), (int16_t)y, (int16_t)(x + s), (int16_t)y,
                     colors.accent, 1, op);
        gfx.drawLine((int16_t)x, (int16_t)(y - s), (int16_t)x, (int16_t)(y + s),
                     colors.accent, 1, op);
    }
}

const VariantDef GREET_VARIANTS[] = {
    {"greet-bow",        "Bow",        2800, TONE_NONE, render_greet_bow},
    {"greet-wave",       "Wave",       2600, TONE_NONE, render_greet_wave},
    {"greet-nod-twice",  "Nod twice",  2400, TONE_NONE, render_greet_nod},
    {"greet-sparkle-hi", "Sparkle hi", 2800, TONE_NONE, render_greet_sparkle},
};

extern const CategoryDef CAT_GREET = {
    "greet", "Greet", ContentKind::EMOTION, TONE_CYAN,
    GREET_VARIANTS, sizeof(GREET_VARIANTS) / sizeof(GREET_VARIANTS[0])
};
