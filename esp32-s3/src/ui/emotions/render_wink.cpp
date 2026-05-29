#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_wink_right(GfxEngine& gfx, float t, const ColorContext& colors) {
    float close = pulse(t, 0.4f, 0.18f);
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    if (close > 0.7f) {
        int16_t hw = (EYE_W - 4) / 2;
        gfx.drawQuadBezier(RX - hw, CY + 4, RX, CY + 4 + 18, RX + hw, CY + 4,
                           colors.eye, 12);
    } else {
        int16_t rh = (int16_t)lerp((float)EYE_H, 4.0f, close);
        gfx.drawEye(RX, CY, EYE_W, rh, EYE_RX, 0, colors.eye);
    }

    if (close > 0.5f) {
        uint8_t op = (uint8_t)(close * 255);
        float s = 7.0f + close * 4.0f;
        gfx.drawLine((int16_t)(RX + 36 - s), CY - 28, (int16_t)(RX + 36 + s), CY - 28,
                     colors.accent, 1, op);
        gfx.drawLine(RX + 36, (int16_t)(CY - 28 - s), RX + 36, (int16_t)(CY - 28 + s),
                     colors.accent, 1, op);
    }
}

static void render_wink_left(GfxEngine& gfx, float t, const ColorContext& colors) {
    float close = pulse(t, 0.4f, 0.18f);

    if (close > 0.7f) {
        int16_t hw = (EYE_W - 4) / 2;
        gfx.drawQuadBezier(LX - hw, CY + 4, LX, CY + 4 + 18, LX + hw, CY + 4,
                           colors.eye, 12);
    } else {
        int16_t lh = (int16_t)lerp((float)EYE_H, 4.0f, close);
        gfx.drawEye(LX, CY, EYE_W, lh, EYE_RX, 0, colors.eye);
    }
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    if (close > 0.5f) {
        uint8_t op = (uint8_t)(close * 255);
        float s = 7.0f + close * 4.0f;
        gfx.drawLine((int16_t)(LX - 36 - s), CY - 28, (int16_t)(LX - 36 + s), CY - 28,
                     colors.accent, 1, op);
        gfx.drawLine(LX - 36, (int16_t)(CY - 28 - s), LX - 36, (int16_t)(CY - 28 + s),
                     colors.accent, 1, op);
    }
}

static void render_wink_double(GfxEngine& gfx, float t, const ColorContext& colors) {
    float close = fmaxf(pulse(t, 0.25f, 0.12f), pulse(t, 0.6f, 0.12f));
    if (close > 0.6f) {
        int16_t hw = (EYE_W - 4) / 2;
        gfx.drawQuadBezier(LX - hw, CY + 4, LX, CY + 22, LX + hw, CY + 4,
                           colors.eye, 12);
        gfx.drawQuadBezier(RX - hw, CY + 4, RX, CY + 22, RX + hw, CY + 4,
                           colors.eye, 12);
    } else {
        int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, close);
        gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
        gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    }
}

static void render_wink_slow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float close = pulse(t, 0.5f, 0.3f);
    int16_t rh = (int16_t)lerp((float)EYE_H, 4.0f, close);
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, rh, EYE_RX, 0, colors.eye);
}

const VariantDef WINK_VARIANTS[] = {
    {"wink-right",  "Right",  2400, TONE_NONE, render_wink_right},
    {"wink-left",   "Left",   2400, TONE_NONE, render_wink_left},
    {"wink-double", "Double", 2800, TONE_NONE, render_wink_double},
    {"wink-slow",   "Slow",   3200, TONE_NONE, render_wink_slow},
};

extern const CategoryDef CAT_WINK = {
    "wink", "Wink", ContentKind::EMOTION, TONE_WARM,
    WINK_VARIANTS, sizeof(WINK_VARIANTS) / sizeof(WINK_VARIANTS[0])
};
