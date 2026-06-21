#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Simple 5-pointed star using triangles (matches JSX starPath).
static void fillStar(GfxEngine& gfx, int16_t cx, int16_t cy, float r1, float r2,
                     uint16_t color, uint8_t alpha = 255) {
    for (int i = 0; i < 5; i++) {
        float a1 = -1.5708f + i * (TAU / 5.0f);
        float a2 = a1 + TAU / 10.0f;
        float a3 = a1 + TAU / 5.0f;
        gfx.fillTriangle((int16_t)(cx + cosf(a1) * r1), (int16_t)(cy + sinf(a1) * r1),
                         (int16_t)(cx + cosf(a2) * r2), (int16_t)(cy + sinf(a2) * r2),
                         cx, cy, color, alpha);
        gfx.fillTriangle((int16_t)(cx + cosf(a2) * r2), (int16_t)(cy + sinf(a2) * r2),
                         (int16_t)(cx + cosf(a3) * r1), (int16_t)(cy + sinf(a3) * r1),
                         cx, cy, color, alpha);
    }
}

// proud-puffed: confident half-lidded eyes + a closed smile
static void render_proud_puffed(GfxEngine& gfx, float t, const ColorContext& colors) {
    float lift = sinf(t * TAU) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.55f);
    int16_t y = (int16_t)(CY - 4 + lift);
    gfx.drawEye(LX, y, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 12, 0, colors.eye);
    // upper-lid bg shadow masking the top of each eye
    int16_t lidBot = (int16_t)(CY - 10 + lift);
    gfx.fillRect(LX - 50, CY - 50, 100, (int16_t)(lidBot - (CY - 50)), colors.bg);
    gfx.fillRect(RX - 50, CY - 50, 100, (int16_t)(lidBot - (CY - 50)), colors.bg);
    // confident closed-smile
    int16_t mx = SCREEN_W / 2, my = SCREEN_H - 30;
    gfx.drawQuadBezier(mx - 30, my, mx, my + 10, mx + 30, my, colors.eye, 5);
}

// proud-glow: soft glow halo behind eyes + a little crown of sparkles
static void render_proud_glow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float p = (sinf(t * TAU) + 1.0f) / 2.0f;
    int16_t r = (int16_t)lerp((float)EYE_W / 2.0f * 1.1f, (float)EYE_W / 2.0f * 1.2f, p);
    uint8_t glowOp = (uint8_t)((0.18f + p * 0.18f) * 255.0f);
    gfx.fillCircle(LX, CY, r, colors.accent, glowOp);
    gfx.fillCircle(RX, CY, r, colors.accent, glowOp);

    int16_t h = (int16_t)(EYE_H * 0.95f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    for (int i = -1; i <= 1; i++) {
        int16_t x = (int16_t)(SCREEN_W / 2 + i * 18);
        int16_t y = (int16_t)(STATUS_H + 22 - (i == 0 ? 0 : 4));
        fillStar(gfx, x, y, 6.0f, 2.5f, colors.accent, 217);
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
