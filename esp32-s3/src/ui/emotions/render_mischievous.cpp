#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// mischievous-grin: narrow eyes with angled brows (inner-high, outer-low), small grin arc
static void render_mischievous_grin(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.8f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.45f);
    int16_t y = (int16_t)(CY + 6 + bob);

    gfx.drawEye(LX, y, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 10, 0, colors.eye);

    // Angled brows: inner-high, outer-low (opposite of angry)
    gfx.drawLine(LX - 34, CY - 26, LX + 30, (int16_t)(CY - 44 + bob), colors.eye, 8);
    gfx.drawLine(RX + 34, CY - 26, RX - 30, (int16_t)(CY - 44 - bob), colors.eye, 8);

    // Small grin arc
    int16_t mx = SCX;
    int16_t my = SCREEN_H - 32;
    gfx.drawQuadBezier(mx - 20, my, mx, my + 12, mx + 20, my,
                       colors.eye, 5);
}

// mischievous-laugh: shake + smile arcs (like happy but with narrower, angled eyes)
static void render_mischievous_laugh(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 5.0f) * 3.0f;
    float sv = cosf(t * TAU * 3.0f) * 1.5f;

    gfx.pushTransform();
    gfx.translate(shake, sv);

    int16_t h = (int16_t)(EYE_H * 0.4f);
    gfx.drawEye(LX, CY + 4, EYE_W, h, 8, 4, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, 8, -4, colors.eye);

    // Angled brows
    gfx.drawLine(LX - 30, CY - 22, LX + 26, CY - 38, colors.eye, 7);
    gfx.drawLine(RX + 30, CY - 22, RX - 26, CY - 38, colors.eye, 7);

    // Smile arcs
    int16_t hw = (EYE_W - 10) / 2;
    gfx.drawQuadBezier(LX - hw, CY + 16, LX, CY + 30, LX + hw, CY + 16,
                       colors.eye, 8);
    gfx.drawQuadBezier(RX - hw, CY + 16, RX, CY + 30, RX + hw, CY + 16,
                       colors.eye, 8);

    gfx.popTransform();
}

// mischievous-side: narrow eyes with pupils shifted to one side, one eyebrow raised
static void render_mischievous_side(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.7f) * 1.0f;
    int16_t h = (int16_t)(EYE_H * 0.5f);
    int16_t y = CY + 4;

    gfx.drawEye(LX, (int16_t)(y + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(y - bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Pupils shifted to one side
    float look = 14.0f + sinf(t * TAU * 0.5f) * 3.0f;
    gfx.fillCircle((int16_t)(LX + look), (int16_t)(y + bob), 7, colors.bg);
    gfx.fillCircle((int16_t)(RX + look), (int16_t)(y - bob), 7, colors.bg);

    // One eyebrow raised (right), other flat (left)
    gfx.drawLine(LX - 30, CY - 30, LX + 30, CY - 30, colors.eye, 7);
    gfx.drawLine(RX - 26, (int16_t)(CY - 36 - bob * 2.0f), RX + 30, CY - 26,
                 colors.eye, 7);
}

// mischievous-twinkle: normal-ish eyes with sparkle star near one eye that twinkles
static void render_mischievous_twinkle(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.8f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY - bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Angled brows (subtle)
    gfx.drawLine(LX - 28, CY - 26, LX + 24, CY - 36, colors.eye, 6);
    gfx.drawLine(RX + 28, CY - 26, RX - 24, CY - 36, colors.eye, 6);

    // Sparkle star near right eye that twinkles
    float twinkle = (sinf(t * TAU * 3.0f) + 1.0f) / 2.0f;
    int16_t sx = RX + 42;
    int16_t sy = CY - 30;
    float s = 4.0f + twinkle * 6.0f;
    uint8_t op = (uint8_t)(twinkle * 255);
    // 4-point star: horizontal + vertical cross
    gfx.drawLine((int16_t)(sx - s), sy, (int16_t)(sx + s), sy,
                 colors.accent, 2, op);
    gfx.drawLine(sx, (int16_t)(sy - s), sx, (int16_t)(sy + s),
                 colors.accent, 2, op);
    // Diagonal lines for extra sparkle
    float sd = s * 0.6f;
    gfx.drawLine((int16_t)(sx - sd), (int16_t)(sy - sd),
                 (int16_t)(sx + sd), (int16_t)(sy + sd),
                 colors.accent, 1, (uint8_t)(op * 0.7f));
    gfx.drawLine((int16_t)(sx + sd), (int16_t)(sy - sd),
                 (int16_t)(sx - sd), (int16_t)(sy + sd),
                 colors.accent, 1, (uint8_t)(op * 0.7f));
}

// --- Category registration ---
const VariantDef MISCHIEVOUS_VARIANTS[] = {
    {"mischievous-grin",    "Grin",    2800, TONE_NONE, render_mischievous_grin},
    {"mischievous-laugh",   "Laugh",   3000, TONE_NONE, render_mischievous_laugh},
    {"mischievous-side",    "Side",    2600, TONE_NONE, render_mischievous_side},
    {"mischievous-twinkle", "Twinkle", 2800, TONE_NONE, render_mischievous_twinkle},
};

extern const CategoryDef CAT_MISCHIEVOUS = {
    "mischievous", "Mischievous", ContentKind::EMOTION, TONE_PURPLE,
    MISCHIEVOUS_VARIANTS, sizeof(MISCHIEVOUS_VARIANTS) / sizeof(MISCHIEVOUS_VARIANTS[0])
};
