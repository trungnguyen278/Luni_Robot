#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// shy-away: droopy eyes with bg pupils looking sideways, blush ellipses
static void render_shy_away(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU * 1.0f) * 3.0f;
    int16_t h = (int16_t)(EYE_H * 0.7f);
    int16_t y = CY + 6;

    gfx.drawEye((int16_t)(LX + sway), y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + sway), y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Bg pupils looking sideways
    float look = sinf(t * TAU * 0.8f) * 16.0f;
    gfx.fillCircle((int16_t)(LX + sway + look), y, 8, colors.bg);
    gfx.fillCircle((int16_t)(RX + sway + look), y, 8, colors.bg);

    // Blush ellipses
    gfx.fillEllipse(LX, CY + 40, 14, 8, colors.accent, 153);
    gfx.fillEllipse(RX, CY + 40, 14, 8, colors.accent, 153);
}

// shy-down: narrow eyes with bg pupils at bottom, blush ellipses
static void render_shy_down(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.7f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.55f);
    int16_t y = (int16_t)(CY + 10 + bob);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Pupils looking down
    gfx.fillCircle(LX, y + 10, 8, colors.bg);
    gfx.fillCircle(RX, y + 10, 8, colors.bg);

    // Blush ellipses
    gfx.fillEllipse(LX, CY + 44, 14, 8, colors.accent, 153);
    gfx.fillEllipse(RX, CY + 44, 14, 8, colors.accent, 153);
}

// shy-peek: narrow eyes that pulse open/close, pupils dart sideways
static void render_shy_peek(GfxEngine& gfx, float t, const ColorContext& colors) {
    float open = (sinf(t * TAU * 0.8f - PI / 2.0f) + 1.0f) / 2.0f;
    int16_t h = (int16_t)lerp(EYE_H * 0.25f, EYE_H * 0.65f, open);

    gfx.drawEye(LX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);

    // Darting pupils when open enough
    if (open > 0.3f) {
        float dart = sinf(t * TAU * 2.5f) * 14.0f;
        uint8_t op = (uint8_t)(open * 255);
        gfx.fillCircle((int16_t)(LX + dart), CY + 4, 6, colors.bg, op);
        gfx.fillCircle((int16_t)(RX + dart), CY + 4, 6, colors.bg, op);
    }

    // Blush
    gfx.fillEllipse(LX, CY + 40, 12, 7, colors.accent, 140);
    gfx.fillEllipse(RX, CY + 40, 12, 7, colors.accent, 140);
}

// shy-blush: normal eyes slightly smaller + prominent blush circles under each eye
static void render_shy_blush(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.8f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.82f);
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Prominent blush circles pulsing
    float blushPulse = (sinf(t * TAU * 1.5f) + 1.0f) / 2.0f;
    int16_t br = (int16_t)lerp(10.0f, 16.0f, blushPulse);
    uint8_t op = (uint8_t)lerp(120.0f, 200.0f, blushPulse);
    gfx.fillCircle(LX - 4, CY + 38, br, colors.accent, op);
    gfx.fillCircle(RX + 4, CY + 38, br, colors.accent, op);
}

// --- Category registration ---
const VariantDef SHY_VARIANTS[] = {
    {"shy-away",  "Away",  2800, TONE_NONE, render_shy_away},
    {"shy-down",  "Down",  2600, TONE_NONE, render_shy_down},
    {"shy-peek",  "Peek",  3000, TONE_NONE, render_shy_peek},
    {"shy-blush", "Blush", 2800, TONE_NONE, render_shy_blush},
};

extern const CategoryDef CAT_SHY = {
    "shy", "Shy", ContentKind::EMOTION, TONE_ROSE,
    SHY_VARIANTS, sizeof(SHY_VARIANTS) / sizeof(SHY_VARIANTS[0])
};
