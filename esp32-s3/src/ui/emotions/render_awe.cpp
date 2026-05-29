#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_awe_gaze(GfxEngine& gfx, float t, const ColorContext& colors) {
    float lift = (sinf(t * TAU - PI / 2.0f) + 1.0f) / 2.0f;
    float sz = 1.0f + lift * 0.15f;
    int16_t w = (int16_t)(EYE_W * 1.08f * sz);
    int16_t h = (int16_t)(EYE_H * 1.05f * sz);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Twinkle stars inside eyes
    gfx.drawLine(LX - 20, CY - 12, LX - 4, CY - 12, colors.bg, 1, 217);
    gfx.drawLine(LX - 12, CY - 20, LX - 12, CY - 4, colors.bg, 1, 217);
    gfx.drawLine(RX - 20, CY - 12, RX - 4, CY - 12, colors.bg, 1, 217);
    gfx.drawLine(RX - 12, CY - 20, RX - 12, CY - 4, colors.bg, 1, 217);

    // Open mouth (wonder)
    int16_t mrx = (int16_t)(5 + lift * 3);
    int16_t mry = (int16_t)(3 + lift * 5);
    gfx.strokeCircle(SCX, SCREEN_H - 30, (mrx + mry) / 2, colors.eye, 3);
}

static void render_awe_rays(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Radial rays from top center
    for (int i = 0; i < 9; i++) {
        float a = -PI + (i / 8.0f) * PI;
        float len = 70.0f + sinf(t * TAU + i) * 6.0f;
        int16_t x1 = (int16_t)(SCX + cosf(a) * 12.0f);
        int16_t y1 = (int16_t)(STATUS_H + 6 + sinf(a) * 12.0f);
        int16_t x2 = (int16_t)(SCX + cosf(a) * len);
        int16_t y2 = (int16_t)(STATUS_H + 6 + sinf(a) * len);
        gfx.drawLine(x1, y1, x2, y2, colors.accent, 2, 115);
    }

    // Wide eyes looking up
    int16_t h = (int16_t)(EYE_H * 0.95f);
    gfx.drawEye(LX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);

    // Pupils tucked at top
    gfx.fillEllipse(LX, CY - 16, 12, 8, colors.bg);
    gfx.fillEllipse(RX, CY - 16, 12, 8, colors.bg);
}

const VariantDef AWE_VARIANTS[] = {
    {"awe-gaze", "Slow gaze",   3600, TONE_NONE, render_awe_gaze},
    {"awe-rays", "Light rays",  3200, TONE_NONE, render_awe_rays},
};

extern const CategoryDef CAT_AWE = {
    "awe", "Awe", ContentKind::EMOTION, TONE_WHITE,
    AWE_VARIANTS, sizeof(AWE_VARIANTS) / sizeof(AWE_VARIANTS[0])
};
