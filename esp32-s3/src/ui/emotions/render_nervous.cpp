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

// nervous-fidget: darting pupils + body jitter + worried wavy mouth
static void render_nervous_fidget(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU * 5.0f) * 10.0f;
    float sh = cosf(t * TAU * 9.0f) * 1.5f;
    gfx.pushTransform();
    gfx.translate(sh, 0.0f);
    int16_t h = (int16_t)(EYE_H * 0.85f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), CY, 9, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), CY, 9, colors.bg);
    int16_t mx = SCREEN_W / 2, my = SCREEN_H - 26;
    gfx.drawQuadBezier(mx - 18, my, mx - 9, my - 4, mx, my, colors.eye, 3);
    gfx.drawQuadBezier(mx, my, mx + 9, my + 4, mx + 18, my, colors.eye, 3);
    gfx.popTransform();
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
