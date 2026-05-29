#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// scared-tremble: shaking wide circle eyes with tiny bg pupils at center
static void render_scared_tremble(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 8.0f) * 3.0f;
    float sv = cosf(t * TAU * 6.0f) * 1.5f;

    gfx.pushTransform();
    gfx.translate(shake, sv);

    // Wide circle eyes
    int16_t r = (int16_t)(EYE_W / 2 * 1.1f);
    gfx.fillCircle(LX, CY, r, colors.eye);
    gfx.fillCircle(RX, CY, r, colors.eye);

    // Pinpoint bg pupils at center
    gfx.fillCircle(LX, CY, 6, colors.bg);
    gfx.fillCircle(RX, CY, 6, colors.bg);

    gfx.popTransform();
}

// scared-shrink: wide eyes with shrinking pupils (phase-based)
static void render_scared_shrink(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = 1.0f + sinf(t * TAU * 0.8f) * 0.08f;
    int16_t w = (int16_t)(EYE_W * s);
    int16_t h = (int16_t)(EYE_H * 1.15f * s);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Shrinking pupils: shrink in first half, grow back in second half
    float phase = (sinf(t * TAU - PI / 2.0f) + 1.0f) / 2.0f;
    int16_t pr = (int16_t)lerp(14.0f, 4.0f, phase);
    gfx.fillCircle(LX, CY, pr, colors.bg);
    gfx.fillCircle(RX, CY, pr, colors.bg);
}

// scared-flinch: pulse-flinch effect, eyes grow briefly then tremble
static void render_scared_flinch(GfxEngine& gfx, float t, const ColorContext& colors) {
    float flinch = pulse(t, 0.3f, 0.15f);
    float tremble = sinf(t * TAU * 10.0f) * 2.0f * (1.0f - flinch);
    float sc = 1.0f + flinch * 0.25f;
    int16_t w = (int16_t)(EYE_W * sc);
    int16_t h = (int16_t)(EYE_H * sc);
    int16_t y = (int16_t)(CY + tremble);

    gfx.drawEye((int16_t)(LX + tremble), y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + tremble), y, w, h, EYE_RX, 0, colors.eye);

    // Bg pupils
    gfx.fillCircle((int16_t)(LX + tremble), y, 8, colors.bg);
    gfx.fillCircle((int16_t)(RX + tremble), y, 8, colors.bg);
}

// scared-darting: eyes sway side to side rapidly with bg pupil circles darting
static void render_scared_darting(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU * 3.0f) * 4.0f;
    int16_t r = (int16_t)(EYE_W / 2 * 1.05f);

    gfx.fillCircle((int16_t)(LX + sway), CY, r, colors.eye);
    gfx.fillCircle((int16_t)(RX + sway), CY, r, colors.eye);

    // Darting pupils: faster frequency than body sway
    float dart = sinf(t * TAU * 5.0f) * 16.0f;
    gfx.fillCircle((int16_t)(LX + sway + dart), CY, 7, colors.bg);
    gfx.fillCircle((int16_t)(RX + sway + dart), CY, 7, colors.bg);
}

// --- Category registration ---
const VariantDef SCARED_VARIANTS[] = {
    {"scared-tremble", "Tremble", 2400, TONE_NONE, render_scared_tremble},
    {"scared-shrink",  "Shrink",  2600, TONE_NONE, render_scared_shrink},
    {"scared-flinch",  "Flinch",  2200, TONE_NONE, render_scared_flinch},
    {"scared-darting", "Darting", 2800, TONE_NONE, render_scared_darting},
};

extern const CategoryDef CAT_SCARED = {
    "scared", "Scared", ContentKind::EMOTION, TONE_WHITE,
    SCARED_VARIANTS, sizeof(SCARED_VARIANTS) / sizeof(SCARED_VARIANTS[0])
};
