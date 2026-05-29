#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// nervous-sweat: normal eyes + sweat drop falling on forehead side (animated y)
static void render_nervous_sweat(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 1.5f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.9f);
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Sweat drop falling on the right side of forehead
    float drop = fmodf(t * 1.2f, 1.0f);
    int16_t sx = RX + 38;
    int16_t sy = (int16_t)lerp((float)(CY - 50), (float)(CY - 10), drop);
    uint8_t op = (uint8_t)((1.0f - drop * 0.6f) * 255.0f);
    // Teardrop: small circle + triangle on top
    int16_t dr = (int16_t)(4.0f + drop * 2.0f);
    gfx.fillCircle(sx, sy, dr, colors.accent, op);
    gfx.fillTriangle(sx - dr + 1, sy - 2, sx + dr - 1, sy - 2,
                     sx, sy - dr * 2, colors.accent, op);
}

// nervous-fidget: eyes sway left-right with slight height variation
static void render_nervous_fidget(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU * 3.0f) * 5.0f;
    float dy = cosf(t * TAU * 4.0f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.88f + dy);

    gfx.drawEye((int16_t)(LX + dx), (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + dx), (int16_t)(CY - dy), EYE_W, h, EYE_RX, 0, colors.eye);
}

// nervous-gulp: normal eyes with throat-bob effect (small circle moving down at center)
static void render_nervous_gulp(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 1.0f) * 1.0f;
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    // Throat bob: small circle that travels down periodically
    float gulp = fmodf(t * 0.8f, 1.0f);
    float gulpPhase = pulse(gulp, 0.5f, 0.25f);
    int16_t ty = (int16_t)lerp((float)(SCREEN_H - 50), (float)(SCREEN_H - 26), gulpPhase);
    uint8_t op = (uint8_t)(gulpPhase * 200.0f);
    gfx.fillCircle(SCX, ty, 5, colors.accent, op);
}

// --- Category registration ---
const VariantDef NERVOUS_VARIANTS[] = {
    {"nervous-sweat",  "Sweat",  2800, TONE_NONE, render_nervous_sweat},
    {"nervous-fidget", "Fidget", 2600, TONE_NONE, render_nervous_fidget},
    {"nervous-gulp",   "Gulp",   2400, TONE_NONE, render_nervous_gulp},
};

extern const CategoryDef CAT_NERVOUS = {
    "nervous", "Nervous", ContentKind::EMOTION, TONE_ORANGE,
    NERVOUS_VARIANTS, sizeof(NERVOUS_VARIANTS) / sizeof(NERVOUS_VARIANTS[0])
};
