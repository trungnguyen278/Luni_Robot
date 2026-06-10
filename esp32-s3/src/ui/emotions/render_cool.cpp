#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// cool-shades: sunglasses (two lenses + bridge + top frame) with a glint + smirk
static void render_cool_shades(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.55f);
    int16_t y = CY + 4;
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // shades: two rounded-rect lenses + bridge + top frame bar
    gfx.fillRoundedRect(LX - 44, CY - 22, 88, 36, 14, colors.accent);
    gfx.fillRoundedRect(RX - 44, CY - 22, 88, 36, 14, colors.accent);
    gfx.fillRect(LX + 44, CY - 6, (int16_t)(RX - LX - 88), 4, colors.accent);
    gfx.drawLine(20, CY - 24, SCREEN_W - 20, CY - 24, colors.accent, 3);

    // glint sliding across the right lens
    float gx = lerp((float)(RX - 30), (float)(RX + 30), fmodf(t * 0.4f, 1.0f));
    gfx.drawLine((int16_t)gx, CY - 16, (int16_t)(gx + 8), CY + 4, colors.bg, 4, 150);

    // tiny smirk below
    int16_t mx = SCREEN_W / 2, my = SCREEN_H - 24;
    gfx.drawQuadBezier(mx - 22, my, mx + 4, my - 8, mx + 24, my + 4, colors.eye, 4);
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

// cool-swag: half-lidded sway + tilt with music notes drifting off the corner
static void render_cool_swag(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU) * 8.0f;
    float tilt = sinf(t * TAU) * 4.0f;
    int16_t h = (int16_t)(EYE_H * 0.5f);

    gfx.pushTransform();
    gfx.translate(sway, 0.0f);
    gfx.rotate(tilt, (float)SCX, (float)CY);
    gfx.drawEye(LX, CY + 6, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 6, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.popTransform();

    for (int i = 0; i < 2; i++) {
        float p = fmodf(t * 1.4f + i * 0.5f, 1.0f);
        int16_t x = (int16_t)(SCREEN_W - 30 + p * 18);
        int16_t y = (int16_t)(STATUS_H + 18 - p * 14);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        gfx.fillEllipse(x, y + 4, 4, 3, colors.accent, op);
        gfx.fillRect(x + 2, y - 10, 2, 16, colors.accent, op);
    }
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
