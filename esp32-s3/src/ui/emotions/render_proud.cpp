#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// proud-puffed: slightly larger eyes, raised position, wide brow lines
static void render_proud_puffed(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breath = sinf(t * TAU * 0.8f) * 2.0f;
    float sc = 1.05f + sinf(t * TAU * 0.6f) * 0.02f;
    int16_t w = (int16_t)(EYE_W * sc);
    int16_t h = (int16_t)(EYE_H * sc);
    int16_t y = (int16_t)(CY - 6 + breath);

    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);

    // Wide brow lines
    gfx.drawLine(LX - 36, (int16_t)(y - 50 + breath * 0.5f),
                 LX + 36, (int16_t)(y - 50 + breath * 0.5f), colors.eye, 8);
    gfx.drawLine(RX - 36, (int16_t)(y - 50 - breath * 0.5f),
                 RX + 36, (int16_t)(y - 50 - breath * 0.5f), colors.eye, 8);
}

// proud-glow: normal eyes + radial glow lines emanating outward from center, soft alpha
static void render_proud_glow(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.9f);
    gfx.drawEye(LX, CY - 4, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY - 4, EYE_W, h, EYE_RX, 0, colors.eye);

    // Radial glow lines from center
    int numRays = 8;
    for (int i = 0; i < numRays; i++) {
        float angle = (float)i / (float)numRays * TAU + t * TAU * 0.3f;
        float p = fmodf(t * 0.8f + (float)i / (float)numRays, 1.0f);
        float innerR = 50.0f + p * 10.0f;
        float outerR = innerR + 18.0f + sinf(t * TAU + (float)i) * 4.0f;
        int16_t x0 = (int16_t)(SCX + cosf(angle) * innerR);
        int16_t y0 = (int16_t)(CY + sinf(angle) * innerR);
        int16_t x1 = (int16_t)(SCX + cosf(angle) * outerR);
        int16_t y1 = (int16_t)(CY + sinf(angle) * outerR);
        uint8_t op = (uint8_t)((0.3f + sinf(t * TAU * 2.0f + (float)i) * 0.2f) * 255);
        gfx.drawLine(x0, y0, x1, y1, colors.accent, 2, op);
    }
}

// proud-medal: eyes looking slightly up + medal shape (circle+ribbon) drawn below
static void render_proud_medal(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.5f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.9f);
    int16_t y = (int16_t)(CY - 8 + bob);

    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Pupils looking slightly up
    gfx.fillCircle(LX, y - 10, 7, colors.bg);
    gfx.fillCircle(RX, y - 10, 7, colors.bg);

    // Medal at bottom center
    float medalBob = sinf(t * TAU * 0.8f) * 2.0f;
    int16_t my = (int16_t)(SCREEN_H - 36 + medalBob);
    // Ribbon (two triangles from top)
    gfx.fillTriangle(SCX - 14, my - 18, SCX, my - 8, SCX - 4, my - 18,
                     colors.accent, 200);
    gfx.fillTriangle(SCX + 14, my - 18, SCX, my - 8, SCX + 4, my - 18,
                     colors.accent, 200);
    // Medal circle
    float medalPulse = 1.0f + sinf(t * TAU * 1.5f) * 0.06f;
    int16_t mr = (int16_t)(10.0f * medalPulse);
    gfx.fillCircle(SCX, my, mr, colors.accent);
    // Star inside medal
    gfx.fillCircle(SCX, my, 4, colors.bg, 200);
}

// proud-rise: eyes bob upward slowly, like chest-puffing
static void render_proud_rise(GfxEngine& gfx, float t, const ColorContext& colors) {
    float rise = sinf(t * TAU * 0.6f) * 8.0f;
    float sc = 1.0f + fmaxf(0.0f, -rise / 8.0f) * 0.04f;
    int16_t w = (int16_t)(EYE_W * sc);
    int16_t h = (int16_t)(EYE_H * sc * 0.9f);
    int16_t y = (int16_t)(CY - 4 + rise);

    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);
}

// --- Category registration ---
const VariantDef PROUD_VARIANTS[] = {
    {"proud-puffed", "Puffed", 2800, TONE_NONE, render_proud_puffed},
    {"proud-glow",   "Glow",   3000, TONE_NONE, render_proud_glow},
    {"proud-medal",  "Medal",  3200, TONE_NONE, render_proud_medal},
    {"proud-rise",   "Rise",   2600, TONE_NONE, render_proud_rise},
};

extern const CategoryDef CAT_PROUD = {
    "proud", "Proud", ContentKind::EMOTION, TONE_WARM,
    PROUD_VARIANTS, sizeof(PROUD_VARIANTS) / sizeof(PROUD_VARIANTS[0])
};
