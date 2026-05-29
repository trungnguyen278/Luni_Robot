#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_dreamy_clouds(GfxEngine& gfx, float t, const ColorContext& colors) {
    float drift = sinf(t * TAU * 0.4f) * 4.0f;
    int16_t h = (int16_t)(EYE_H * 0.75f);
    int16_t y = (int16_t)(CY - 2 + drift);
    gfx.drawEye(LX, y, EYE_W, h, 28, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 28, 0, colors.eye);

    // Twinkle highlights in eyes
    gfx.fillCircle(LX - 14, (int16_t)(CY - 8 + drift), 5, colors.bg, 217);
    gfx.fillCircle(RX - 14, (int16_t)(CY - 8 + drift), 5, colors.bg, 217);

    // Clouds drifting across top
    for (int i = 0; i < 3; i++) {
        float seed = i * 0.33f;
        float p = fmodf(t * 0.5f + seed, 1.0f);
        float x = -40.0f + p * (SCREEN_W + 80.0f);
        float cy2 = (float)(STATUS_H + 8 + (i % 2) * 6);
        gfx.fillEllipse((int16_t)x, (int16_t)cy2, 10, 5, colors.accent, 178);
        gfx.fillEllipse((int16_t)(x + 8), (int16_t)(cy2 - 3), 8, 5, colors.accent, 178);
        gfx.fillEllipse((int16_t)(x - 8), (int16_t)(cy2 - 2), 7, 4, colors.accent, 178);
    }
}

static void render_dreamy_bubbles(GfxEngine& gfx, float t, const ColorContext& colors) {
    float lookUp = sinf(t * TAU * 0.5f) * 4.0f - 8.0f;
    int16_t h = (int16_t)(EYE_H * 0.7f);
    gfx.drawEye(LX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, EYE_RX, 0, colors.eye);

    // Pupils looking up-right
    gfx.fillCircle(LX + 8, (int16_t)(CY + 4 + lookUp), 10, colors.bg);
    gfx.fillCircle(RX + 8, (int16_t)(CY + 4 + lookUp), 10, colors.bg);

    // Rising thought bubbles
    for (int i = 0; i < 4; i++) {
        float seed = i * 0.25f;
        float p = fmodf(t * 0.7f + seed, 1.0f);
        float x = (float)(RX + 30) + p * 24.0f;
        float y = lerp((float)(CY - 20), (float)(STATUS_H + 8), p);
        int16_t r = (int16_t)lerp(3.0f, 14.0f, p);
        uint8_t op = (uint8_t)((1.0f - p * 0.4f) * 255);
        gfx.strokeCircle((int16_t)x, (int16_t)y, r, colors.accent, 2);
    }
}

static void render_dreamy_sparkle(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye(LX, CY, EYE_W, h, 28, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 28, 0, colors.eye);

    // Hollow centers
    gfx.fillCircle(LX, CY, 14, colors.bg, 153);
    gfx.fillCircle(RX, CY, 14, colors.bg, 153);

    // Distant sparkle field
    int xpos[] = {30, 77, 124, 171, 218, 265, 50};
    int ypos[] = {34, 48, 58, 42, 52, 38, 74};
    for (int i = 0; i < 7; i++) {
        float seed = fmodf(i * 53.0f, 100.0f) / 100.0f;
        float p = fmodf(t * 0.8f + seed, 1.0f);
        float s = 2.0f + fabsf(sinf(p * TAU)) * 3.0f;
        uint8_t op = (uint8_t)((0.4f + fabsf(sinf(p * TAU)) * 0.5f) * 255);
        // 4-point star
        gfx.drawLine((int16_t)(xpos[i] - s), (int16_t)(STATUS_H + ypos[i]),
                     (int16_t)(xpos[i] + s), (int16_t)(STATUS_H + ypos[i]),
                     colors.accent, 1, op);
        gfx.drawLine(xpos[i], (int16_t)(STATUS_H + ypos[i] - s),
                     xpos[i], (int16_t)(STATUS_H + ypos[i] + s),
                     colors.accent, 1, op);
    }
}

const VariantDef DREAMY_VARIANTS[] = {
    {"dreamy-clouds",  "Floating clouds",  4400, TONE_NONE, render_dreamy_clouds},
    {"dreamy-bubbles", "Thought bubbles",  3800, TONE_NONE, render_dreamy_bubbles},
    {"dreamy-sparkle", "Distant sparkle",  3000, TONE_NONE, render_dreamy_sparkle},
};

extern const CategoryDef CAT_DREAMY = {
    "dreamy", "Dreamy", ContentKind::EMOTION, TONE_PURPLE,
    DREAMY_VARIANTS, sizeof(DREAMY_VARIANTS) / sizeof(DREAMY_VARIANTS[0])
};
