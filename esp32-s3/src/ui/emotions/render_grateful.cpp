#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void fillHeart(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                      uint16_t color, uint8_t alpha = 255) {
    float k = s / 30.0f;
    int16_t r = (int16_t)(9.0f * k);
    int16_t bumpY = (int16_t)(cy - 9.0f * k);
    int16_t tipY  = (int16_t)(cy + 9.0f * k);
    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

static void fillStar4(GfxEngine& gfx, int16_t cx, int16_t cy, float r1, float r2,
                      uint16_t color, uint8_t alpha = 255) {
    for (int i = 0; i < 4; i++) {
        float a0 = i * PI / 2.0f - PI / 4.0f;
        float a1 = a0 + PI / 4.0f;
        gfx.fillTriangle(
            cx, cy,
            (int16_t)(cx + cosf(a0) * r1), (int16_t)(cy + sinf(a0) * r1),
            (int16_t)(cx + cosf(a1) * r2), (int16_t)(cy + sinf(a1) * r2),
            color, alpha);
    }
}

static void render_grateful_bow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU - PI / 2.0f);
    float tilt = lerp(0.0f, 22.0f, (phase + 1.0f) / 2.0f);
    float dy = lerp(0.0f, 14.0f, (phase + 1.0f) / 2.0f);

    gfx.pushTransform();
    gfx.rotate(tilt, (float)SCX, (float)(SCREEN_H + 60));
    gfx.translate(0, dy);

    // Closed smile arcs
    int16_t hw = (EYE_W - 4) / 2;
    gfx.drawQuadBezier(LX - hw, CY + 4, LX, CY + 4 + 22, LX + hw, CY + 4,
                       colors.eye, 12);
    gfx.drawQuadBezier(RX - hw, CY + 4, RX, CY + 4 + 22, RX + hw, CY + 4,
                       colors.eye, 12);

    // Small heart between eyes
    fillHeart(gfx, SCX, CY - 6, 18.0f, colors.accent);

    // Gentle mouth
    gfx.pushAlpha(178);
    gfx.drawQuadBezier(SCX - 14, SCREEN_H - 32, SCX, SCREEN_H - 27,
                       SCX + 14, SCREEN_H - 32, colors.eye, 4);
    gfx.popAlpha();

    gfx.popTransform();
}

static void render_grateful_sparkle(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.8f);
    gfx.drawEye(LX, CY - 4, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY - 4, EYE_W, h, EYE_RX, 0, colors.eye);

    // Heart cheek pads
    fillHeart(gfx, LX, CY + 34, 12.0f, colors.accent, 217);
    fillHeart(gfx, RX, CY + 34, 12.0f, colors.accent, 217);

    // Rising sparkles
    for (int i = 0; i < 4; i++) {
        float seed = i * 0.25f;
        float p = fmodf(t + seed, 1.0f);
        float baseX = 40.0f + i * 80.0f;
        float x = baseX + sinf(p * TAU + i) * 6.0f;
        float y = lerp((float)(SCREEN_H - 30), (float)(STATUS_H + 12), p);
        float s = lerp(8.0f, 2.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 255);
        fillStar4(gfx, (int16_t)x, (int16_t)y, s, s * 0.4f, colors.accent, op);
    }

    gfx.drawText("THANK YOU", SCX, SCREEN_H - 14, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 191);
}

const VariantDef GRATEFUL_VARIANTS[] = {
    {"grateful-bow",     "Deep bow",      3200, TONE_NONE, render_grateful_bow},
    {"grateful-sparkle", "Heart sparkle", 2400, TONE_NONE, render_grateful_sparkle},
};

extern const CategoryDef CAT_GRATEFUL = {
    "grateful", "Grateful", ContentKind::EMOTION, TONE_ROSE,
    GRATEFUL_VARIANTS, sizeof(GRATEFUL_VARIANTS) / sizeof(GRATEFUL_VARIANTS[0])
};
