#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// embarrassed-blush: slightly squished eyes + large blush circles pulsing
static void render_embarrassed_blush(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sq = sinf(t * TAU * 1.2f) * 0.04f;
    int16_t h = (int16_t)(EYE_H * (0.72f + sq));
    int16_t y = CY + 4;
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Large pulsing blush circles
    float blushPulse = (sinf(t * TAU * 2.0f) + 1.0f) / 2.0f;
    int16_t br = (int16_t)lerp(12.0f, 20.0f, blushPulse);
    uint8_t op = (uint8_t)lerp(140.0f, 220.0f, blushPulse);
    gfx.fillCircle(LX - 6, CY + 40, br, colors.accent, op);
    gfx.fillCircle(RX + 6, CY + 40, br, colors.accent, op);
}

// embarrassed-hide: eyes get very narrow as if hiding, small blush marks
static void render_embarrassed_hide(GfxEngine& gfx, float t, const ColorContext& colors) {
    float hidePhase = (sinf(t * TAU * 0.7f - PI / 2.0f) + 1.0f) / 2.0f;
    int16_t h = (int16_t)lerp(EYE_H * 0.6f, EYE_H * 0.3f, hidePhase);
    int16_t y = (int16_t)(CY + 6 + hidePhase * 4.0f);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Small blush marks
    uint8_t op = (uint8_t)lerp(100.0f, 180.0f, hidePhase);
    gfx.fillCircle(LX - 4, CY + 36, 8, colors.accent, op);
    gfx.fillCircle(RX + 4, CY + 36, 8, colors.accent, op);
}

// embarrassed-cringe: asymmetric eyes (one squinting more), slight sway
static void render_embarrassed_cringe(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU * 1.5f) * 3.0f;
    float squint = (sinf(t * TAU * 1.0f) + 1.0f) / 2.0f;

    int16_t lh = (int16_t)lerp(EYE_H * 0.7f, EYE_H * 0.4f, squint);
    int16_t rh = (int16_t)(EYE_H * 0.65f);

    gfx.drawEye((int16_t)(LX + sway), CY + 2, EYE_W, lh, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + sway), CY + 2, EYE_W, rh, EYE_RX, 0, colors.eye);

    // Blush marks
    gfx.fillCircle((int16_t)(LX + sway - 4), CY + 38, 10, colors.accent, 160);
    gfx.fillCircle((int16_t)(RX + sway + 4), CY + 38, 10, colors.accent, 160);
}

// --- Category registration ---
const VariantDef EMBARRASSED_VARIANTS[] = {
    {"embarrassed-blush",  "Blush",  2800, TONE_NONE, render_embarrassed_blush},
    {"embarrassed-hide",   "Hide",   3000, TONE_NONE, render_embarrassed_hide},
    {"embarrassed-cringe", "Cringe", 2600, TONE_NONE, render_embarrassed_cringe},
};

extern const CategoryDef CAT_EMBARRASSED = {
    "embarrassed", "Embarrassed", ContentKind::EMOTION, TONE_ROSE,
    EMBARRASSED_VARIANTS, sizeof(EMBARRASSED_VARIANTS) / sizeof(EMBARRASSED_VARIANTS[0])
};
