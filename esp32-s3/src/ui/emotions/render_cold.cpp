#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_cold_shiver(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 10.0f) * 3.0f;
    float sv = cosf(t * TAU * 10.0f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.pushTransform();
    gfx.translate(sh, sv);
    gfx.drawEye(LX, CY, EYE_W, h, 16, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 16, 0, colors.eye);
    gfx.popTransform();

    // Chattering teeth zigzag
    int16_t zx = SCX - 24, zy = SCREEN_H - 30;
    for (int i = 0; i < 6; i++) {
        int16_t x0 = (int16_t)(zx + i * 6);
        int16_t x1 = (int16_t)(zx + (i + 1) * 6);
        int16_t y0 = (int16_t)(zy + (i % 2 == 0 ? 0 : 4));
        int16_t y1 = (int16_t)(zy + ((i + 1) % 2 == 0 ? 0 : 4));
        gfx.drawLine(x0, y0, x1, y1, colors.eye, 3);
    }
}

static void render_cold_snowflakes(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 8.0f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye((int16_t)(LX + sh), CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + sh), CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Drifting snowflakes
    for (int i = 0; i < 5; i++) {
        float seed = i * 0.21f;
        float p = fmodf(t + seed, 1.0f);
        float x = 30.0f + fmodf(i * 56.0f, SCREEN_W - 60.0f) + sinf(p * TAU + i) * 8.0f;
        float y = lerp((float)(STATUS_H + 4), (float)(SCREEN_H - 8), p);
        float r = 3.0f + (i % 2) * 1.0f;
        uint8_t op = (uint8_t)((1.0f - p * 0.3f) * 255);

        gfx.pushTransform();
        gfx.translate(x, y);
        gfx.rotate(p * 360.0f, 0, 0);
        gfx.drawLine((int16_t)(-r), 0, (int16_t)r, 0, colors.eye, 1, op);
        gfx.drawLine(0, (int16_t)(-r), 0, (int16_t)r, colors.eye, 1, op);
        int16_t rd = (int16_t)(r * 0.7f);
        gfx.drawLine(-rd, -rd, rd, rd, colors.eye, 1, (uint8_t)(op * 0.7f));
        gfx.drawLine(rd, -rd, -rd, rd, colors.eye, 1, (uint8_t)(op * 0.7f));
        gfx.popTransform();
    }
}

const VariantDef COLD_VARIANTS[] = {
    {"cold-shiver",     "Shiver",     700,  TONE_NONE, render_cold_shiver},
    {"cold-snowflakes", "Snowflakes", 3000, TONE_NONE, render_cold_snowflakes},
};

extern const CategoryDef CAT_COLD = {
    "cold", "Cold", ContentKind::EMOTION, TONE_BLUE,
    COLD_VARIANTS, sizeof(COLD_VARIANTS) / sizeof(COLD_VARIANTS[0])
};
