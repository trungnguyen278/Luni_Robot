#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Shared: draw angled bg lids for smug look (diagonal lines from top-outer to inner)
static void drawSmugLids(GfxEngine& gfx, float t, uint16_t color) {
    float sh = sinf(t * TAU * 1.0f) * 1.0f;
    // Left lid: outer-high to inner-low
    gfx.drawLine(LX - 40, (int16_t)(CY - 30 + sh), LX + 20, CY - 10, color, 8);
    // Right lid: outer-high to inner-low
    gfx.drawLine(RX + 40, (int16_t)(CY - 30 - sh), RX - 20, CY - 10, color, 8);
}

// smug-smirk: narrow eyes with angled bg lids, asymmetric smile arc at bottom
static void render_smug_smirk(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.8f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.45f);
    int16_t y = (int16_t)(CY + 8 + bob);

    gfx.drawEye(LX, y, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 10, 0, colors.eye);
    drawSmugLids(gfx, t, colors.eye);

    // Asymmetric smile arc: right side higher
    int16_t mx = SCX + 10;
    int16_t my = SCREEN_H - 32;
    gfx.drawQuadBezier(mx - 28, my + 4, mx, my + 14, mx + 22, my - 2,
                       colors.eye, 5);
}

// smug-side: narrow eyes with bg pupils looking far to one side, angled bg lids
static void render_smug_side(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.6f) * 1.0f;
    int16_t h = (int16_t)(EYE_H * 0.6f);
    int16_t y = (int16_t)(CY + 4 + bob);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    drawSmugLids(gfx, t, colors.eye);

    // Pupils looking far to one side (right)
    float look = 18.0f + sinf(t * TAU * 0.5f) * 2.0f;
    gfx.fillCircle((int16_t)(LX + look), y, 7, colors.bg);
    gfx.fillCircle((int16_t)(RX + look), y, 7, colors.bg);
}

// smug-brow: eyes with tilted brow lines above (one raised), slight tilt
static void render_smug_brow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU * 0.7f) * 3.0f;
    int16_t h = (int16_t)(EYE_H * 0.6f);

    gfx.drawEye(LX, CY + 4, EYE_W, h, EYE_RX, tilt, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, EYE_RX, -tilt, colors.eye);

    // Left brow: flat/slightly angled
    gfx.drawLine(LX - 32, CY - 36, LX + 32, (int16_t)(CY - 40 + tilt), colors.eye, 8);
    // Right brow: raised higher
    gfx.drawLine(RX - 32, (int16_t)(CY - 44 - tilt), RX + 32, CY - 36, colors.eye, 8);
}

// smug-blink: slow deliberate blink with narrowed eyes
static void render_smug_blink(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Slow blink around t=0.5
    float close = pulse(t, 0.5f, 0.2f);
    int16_t baseH = (int16_t)(EYE_H * 0.5f);
    int16_t h = (int16_t)lerp((float)baseH, 4.0f, close);

    gfx.drawEye(LX, CY + 6, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, CY + 6, EYE_W, h, 12, 0, colors.eye);

    // Subtle smirk
    if (close < 0.3f) {
        gfx.drawQuadBezier(SCX - 18, SCREEN_H - 34, SCX + 6, SCREEN_H - 24,
                           SCX + 22, SCREEN_H - 36, colors.eye, 4);
    }
}

// --- Category registration ---
const VariantDef SMUG_VARIANTS[] = {
    {"smug-smirk", "Smirk", 2800, TONE_NONE, render_smug_smirk},
    {"smug-side",  "Side",  2600, TONE_NONE, render_smug_side},
    {"smug-brow",  "Brow",  2800, TONE_NONE, render_smug_brow},
    {"smug-blink", "Blink", 2400, TONE_NONE, render_smug_blink},
};

extern const CategoryDef CAT_SMUG = {
    "smug", "Smug", ContentKind::EMOTION, TONE_WARM,
    SMUG_VARIANTS, sizeof(SMUG_VARIANTS) / sizeof(SMUG_VARIANTS[0])
};
