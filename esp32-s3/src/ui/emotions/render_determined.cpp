#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Shared V-brow lines for determined expressions
static void drawDeterminedBrows(GfxEngine& gfx, float t, uint16_t color) {
    float sh = sinf(t * TAU * 3.0f) * 1.0f;
    // Strong V-brow: outer-high to inner-low
    gfx.drawLine(LX - 36, (int16_t)(CY - 48 + sh), LX + 28, CY - 30, color, 8);
    gfx.drawLine(RX + 36, (int16_t)(CY - 48 - sh), RX - 28, CY - 30, color, 8);
}

// determined-stare: narrow angled eyes with strong V-brow lines above
static void render_determined_stare(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 2.0f) * 0.5f;
    int16_t h = (int16_t)(EYE_H * 0.5f);
    gfx.drawEye((int16_t)(LX + sh), CY, EYE_W, h, 8, 0, colors.eye);
    gfx.drawEye((int16_t)(RX - sh), CY, EYE_W, h, 8, 0, colors.eye);
    drawDeterminedBrows(gfx, t, colors.eye);
}

// determined-fire: narrow eyes + small flame shapes flickering above head center
static void render_determined_fire(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.5f);
    gfx.drawEye(LX, CY, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 10, 0, colors.eye);
    drawDeterminedBrows(gfx, t, colors.eye);

    // Flame shapes above center
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.8f + i * 0.33f, 1.0f);
        float flicker = sinf(t * TAU * 6.0f + i * 2.0f) * 4.0f;

        int16_t fx = (int16_t)(SCX - 16 + i * 16 + flicker);
        int16_t baseY = (int16_t)(STATUS_H + 40 - p * 20.0f);
        float sz = lerp(10.0f, 4.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 220.0f);

        // Flame as a small triangle pointing up
        gfx.fillTriangle(
            (int16_t)(fx - sz * 0.5f), baseY,
            (int16_t)(fx + sz * 0.5f), baseY,
            fx, (int16_t)(baseY - sz * 1.4f),
            colors.accent, op
        );
    }
}

// determined-lock: narrow eyes that briefly pulse narrower, "locked on" feel
static void render_determined_lock(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Lock pulse: periodic narrow squeeze
    float cyc = fmodf(t * 0.8f, 1.0f);
    float lock = 0.0f;
    if (cyc > 0.4f && cyc < 0.6f) {
        lock = sinf((cyc - 0.4f) / 0.2f * PI);
    }

    float hScale = lerp(0.5f, 0.3f, lock);
    int16_t h = (int16_t)(EYE_H * hScale);
    int16_t rx = (int16_t)lerp(10.0f, 6.0f, lock);

    gfx.drawEye(LX, CY, EYE_W, h, rx, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, rx, 0, colors.eye);
    drawDeterminedBrows(gfx, t, colors.eye);

    // Lock indicator: small brackets around eyes during lock pulse
    if (lock > 0.1f) {
        uint8_t op = (uint8_t)(lock * 200.0f);
        // Left eye brackets
        gfx.drawLine(LX - EYE_W / 2 - 6, CY - h / 2, LX - EYE_W / 2 - 6, CY + h / 2,
                     colors.accent, 2, op);
        gfx.drawLine(LX + EYE_W / 2 + 6, CY - h / 2, LX + EYE_W / 2 + 6, CY + h / 2,
                     colors.accent, 2, op);
        // Right eye brackets
        gfx.drawLine(RX - EYE_W / 2 - 6, CY - h / 2, RX - EYE_W / 2 - 6, CY + h / 2,
                     colors.accent, 2, op);
        gfx.drawLine(RX + EYE_W / 2 + 6, CY - h / 2, RX + EYE_W / 2 + 6, CY + h / 2,
                     colors.accent, 2, op);
    }
}

// --- Category registration ---
const VariantDef DETERMINED_VARIANTS[] = {
    {"determined-stare", "Stare", 2800, TONE_NONE, render_determined_stare},
    {"determined-fire",  "Fire",  3000, TONE_NONE, render_determined_fire},
    {"determined-lock",  "Lock",  2600, TONE_NONE, render_determined_lock},
};

extern const CategoryDef CAT_DETERMINED = {
    "determined", "Determined", ContentKind::EMOTION, TONE_RED,
    DETERMINED_VARIANTS, sizeof(DETERMINED_VARIANTS) / sizeof(DETERMINED_VARIANTS[0])
};
