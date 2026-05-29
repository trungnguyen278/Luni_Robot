#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// cool-shades: narrow eyes with horizontal line across middle (sunglasses bar)
static void render_cool_shades(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.5f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.45f);
    int16_t y = (int16_t)(CY + 4 + bob);

    gfx.drawEye(LX, y, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 10, 0, colors.eye);

    // Sunglasses bar: continuous line across both eyes (no gap)
    int16_t barY = y;
    gfx.drawLine(LX - EYE_W / 2 - 6, barY, RX + EYE_W / 2 + 6, barY,
                 colors.eye, 6);

    // Highlight glint
    float glintX = lerp((float)(LX - 20), (float)(RX + 20),
                        fmodf(t * 0.4f, 1.0f));
    gfx.fillCircle((int16_t)glintX, barY - 4, 3, colors.bg, 140);
}

// cool-smirk: narrow eyes, one slightly more closed, small asymmetric smile
static void render_cool_smirk(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.6f) * 1.0f;
    int16_t lh = (int16_t)(EYE_H * 0.5f);
    int16_t rh = (int16_t)(EYE_H * 0.4f);
    int16_t y = (int16_t)(CY + 6 + bob);

    gfx.drawEye(LX, y, EYE_W, lh, 12, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, rh, 10, 0, colors.eye);

    // Small asymmetric smile (right side higher)
    int16_t mx = SCX + 8;
    int16_t my = SCREEN_H - 34;
    gfx.drawQuadBezier(mx - 22, my + 2, mx, my + 10, mx + 18, my - 4,
                       colors.eye, 4);
}

// cool-swag: eyes sway side to side smoothly, slightly narrowed
static void render_cool_swag(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU * 0.6f) * 8.0f;
    float tilt = sinf(t * TAU * 0.6f) * 3.0f;
    int16_t h = (int16_t)(EYE_H * 0.55f);

    gfx.drawEye((int16_t)(LX + sway), CY + 4, EYE_W, h, 14, tilt, colors.eye);
    gfx.drawEye((int16_t)(RX + sway), CY + 4, EYE_W, h, 14, -tilt, colors.eye);
}

// --- Category registration ---
const VariantDef COOL_VARIANTS[] = {
    {"cool-shades", "Shades", 3000, TONE_NONE, render_cool_shades},
    {"cool-smirk",  "Smirk",  2800, TONE_NONE, render_cool_smirk},
    {"cool-swag",   "Swag",   3200, TONE_NONE, render_cool_swag},
};

extern const CategoryDef CAT_COOL = {
    "cool", "Cool", ContentKind::EMOTION, TONE_CYAN,
    COOL_VARIANTS, sizeof(COOL_VARIANTS) / sizeof(COOL_VARIANTS[0])
};
