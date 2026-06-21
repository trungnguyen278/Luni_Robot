#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Filled heart: two circles for bumps + triangle for point
static void fillHeart(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                      uint16_t color, uint8_t alpha = 255) {
    float k = s / 30.0f;
    int16_t r = (int16_t)(9.0f * k);
    int16_t bumpY = (int16_t)(cy - 9.0f * k);
    int16_t tipY = (int16_t)(cy + 9.0f * k);

    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    // Triangle from left-of-left-circle to right-of-right-circle to tip
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

// love-hearts: heart-shaped eyes that pulse
static void render_love_hearts(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = (1.0f + sinf(t * TAU * 2.0f) * 0.1f) * (float)EYE_W * 0.95f;
    fillHeart(gfx, LX, CY, s, colors.eye);
    fillHeart(gfx, RX, CY, s, colors.eye);
}

// love-floating: normal eyes + floating hearts rising
static void render_love_floating(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.85f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    for (int i = 0; i < 4; i++) {
        float p = fmodf(t + i * 0.25f, 1.0f);
        float x = 40.0f + i * 80.0f + sinf(p * TAU + (float)i) * 8.0f;
        float y = lerp((float)(SCREEN_H - 30), (float)(STATUS_H + 6), p);
        float sz = 14.0f + (1.0f - p) * 4.0f;
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        fillHeart(gfx, (int16_t)x, (int16_t)y, sz, colors.accent, op);
    }
}

// love-wink: left heart eye + right winks
static void render_love_wink(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sz = (float)EYE_W * 0.9f;
    float close = pulse(t, 0.5f, 0.2f);

    fillHeart(gfx, LX, CY, sz, colors.eye);

    if (close > 0.5f) {
        // Winking: draw smile arc for right eye
        int16_t hw = (EYE_W - 6) / 2;
        gfx.drawQuadBezier(RX - hw, CY + 6, RX, CY + 6 + 18, RX + hw, CY + 6,
                           colors.eye, 12);
    } else {
        fillHeart(gfx, RX, CY, sz * (1.0f - close * 0.3f), colors.eye);
    }
}

// love-blush: heart eyes + flat pink blush streaks under each eye
static void render_love_blush(GfxEngine& gfx, float t, const ColorContext& colors) {
    float p2 = 1.0f + sinf(t * TAU * 2.0f) * 0.06f;
    float s = (float)EYE_W * 0.85f * p2;
    fillHeart(gfx, LX, CY - 6, s, colors.eye);
    fillHeart(gfx, RX, CY - 6, s, colors.eye);
    gfx.fillEllipse(LX, CY + 42, 16, 5, colors.accent, 153);
    gfx.fillEllipse(RX, CY + 42, 16, 5, colors.accent, 153);
}

// love-shower: normal eyes + falling heart shapes from top
static void render_love_shower(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.9f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    for (int i = 0; i < 8; i++) {
        float p = fmodf(t + i * 0.125f, 1.0f);
        float x = 24.0f + i * 38.0f + sinf(p * TAU + (float)i) * 6.0f;
        float y = lerp((float)(STATUS_H + 4), (float)(SCREEN_H - 4), p);
        float sz = 10.0f + (i % 3) * 4.0f;
        uint8_t op = (uint8_t)((1.0f - p) * 0.9f * 255.0f);
        fillHeart(gfx, (int16_t)x, (int16_t)y, sz * (1.0f - p * 0.3f), colors.accent, op);
    }
}

// --- Category registration ---
const VariantDef LOVE_VARIANTS[] = {
    {"love-hearts",   "Heart eyes",      1600, TONE_NONE, render_love_hearts},
    {"love-floating", "Floating hearts", 2400, TONE_NONE, render_love_floating},
    {"love-wink",     "Heart wink",      2400, TONE_NONE, render_love_wink},
    {"love-blush",    "Blush",           2800, TONE_NONE, render_love_blush},
    {"love-shower",   "Heart shower",    3000, TONE_NONE, render_love_shower},
};

extern const CategoryDef CAT_LOVE = {
    "love", "Love", ContentKind::EMOTION, TONE_ROSE,
    LOVE_VARIANTS, sizeof(LOVE_VARIANTS) / sizeof(LOVE_VARIANTS[0])
};
