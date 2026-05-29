#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// sleepy-heavy: very narrow drooping eyes
static void render_sleepy_heavy(GfxEngine& gfx, float t, const ColorContext& colors) {
    float droop = sinf(t * TAU * 0.6f) * 4.0f;
    int16_t h = (int16_t)fmaxf(18.0f + droop, 8.0f);
    gfx.drawEye(LX, CY + 28, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, CY + 28, EYE_W, h, 12, 0, colors.eye);
}

// sleepy-yawn: narrow eyes + mouth ellipse yawn
static void render_sleepy_yawn(GfxEngine& gfx, float t, const ColorContext& colors) {
    float yawn = pulse(t, 0.55f, 0.18f);
    int16_t lidH = (int16_t)lerp(24.0f, 8.0f, yawn);
    gfx.drawEye(LX, CY + 22, EYE_W, lidH, 10, 0, colors.eye);
    gfx.drawEye(RX, CY + 22, EYE_W, lidH, 10, 0, colors.eye);

    if (yawn > 0.03f) {
        int16_t mRx = (int16_t)(8.0f + yawn * 4.0f);
        int16_t mRy = (int16_t)(6.0f + yawn * 12.0f);
        int16_t mx = SCREEN_W / 2;
        int16_t my = SCREEN_H - 36;
        gfx.fillEllipse(mx, my, mRx, mRy, colors.eye);
        gfx.fillEllipse(mx, my, (int16_t)(mRx - 4), (int16_t)(mRy - 4), colors.bg);
    }
}

// sleepy-nod: drowsy nodding, eyes drift down and back up slowly
static void render_sleepy_nod(GfxEngine& gfx, float t, const ColorContext& colors) {
    float nod = sinf(t * TAU * 0.5f);
    float dy = nod * 14.0f;
    float lidClose = clamp((nod + 1.0f) / 2.0f, 0.0f, 1.0f);
    int16_t h = (int16_t)lerp(8.0f, (float)EYE_H * 0.6f, 1.0f - lidClose);
    gfx.drawEye(LX, (int16_t)(CY + 20 + dy), EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + 20 + dy), EYE_W, h, 12, 0, colors.eye);
}

// sleepy-blink-slow: very slow heavy blinks with long close duration
static void render_sleepy_blink_slow(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Slow blink: eyes close for a long portion of the cycle
    float phase = fmodf(t, 1.0f);
    float openness;
    if (phase < 0.3f) {
        openness = 1.0f - ease::inOut(phase / 0.3f);
    } else if (phase < 0.6f) {
        openness = 0.0f;
    } else {
        openness = ease::inOut((phase - 0.6f) / 0.4f);
    }
    int16_t h = (int16_t)lerp(6.0f, (float)EYE_H * 0.65f, openness);
    gfx.drawEye(LX, CY + 24, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, CY + 24, EYE_W, h, 12, 0, colors.eye);
}

// --- Category registration ---
const VariantDef SLEEPY_VARIANTS[] = {
    {"sleepy-heavy",      "Heavy lids",  4000, TONE_NONE, render_sleepy_heavy},
    {"sleepy-yawn",       "Yawn",        2800, TONE_NONE, render_sleepy_yawn},
    {"sleepy-nod",        "Nod off",     3200, TONE_NONE, render_sleepy_nod},
    {"sleepy-blink-slow", "Slow blink",  4400, TONE_NONE, render_sleepy_blink_slow},
};

extern const CategoryDef CAT_SLEEPY = {
    "sleepy", "Sleepy", ContentKind::EMOTION, TONE_PURPLE,
    SLEEPY_VARIANTS, sizeof(SLEEPY_VARIANTS) / sizeof(SLEEPY_VARIANTS[0])
};
