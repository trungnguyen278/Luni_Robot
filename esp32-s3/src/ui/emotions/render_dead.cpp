#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Draw X-shaped eye
static void drawXEye(GfxEngine& gfx, int16_t cx, int16_t cy, int16_t sz,
                     uint16_t color, uint8_t alpha) {
    gfx.drawLine(cx - sz, cy - sz, cx + sz, cy + sz, color, 10, alpha);
    gfx.drawLine(cx + sz, cy - sz, cx - sz, cy + sz, color, 10, alpha);
}

// dead-x: X eyes with flicker + frown
static void render_dead_x(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint8_t alpha = (t > 0.92f) ? 77 : 255; // flicker at end
    drawXEye(gfx, LX, CY, 28, colors.eye, alpha);
    drawXEye(gfx, RX, CY, 28, colors.eye, alpha);

    // Frown arc
    int16_t mx = SCREEN_W / 2;
    gfx.drawQuadBezier(mx - 30, SCREEN_H - 32, mx, SCREEN_H - 32 + 8,
                       mx + 30, SCREEN_H - 32, colors.eye, 5);
}

// dead-fade: X eyes with slow fade-blink
static void render_dead_fade(GfxEngine& gfx, float t, const ColorContext& colors) {
    float op = 0.4f + fabsf(sinf(t * TAU * 0.5f)) * 0.6f;
    uint8_t alpha = (uint8_t)(op * 255.0f);
    drawXEye(gfx, LX, CY, 24, colors.eye, alpha);
    drawXEye(gfx, RX, CY, 24, colors.eye, alpha);
}

// dead-glitch: X eyes flicker/glitch (appear and disappear rapidly)
static void render_dead_glitch(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Pseudo-random glitch using fast sin
    float g1 = sinf(t * 73.0f);
    float g2 = sinf(t * 127.0f);
    bool showL = g1 > -0.3f;
    bool showR = g2 > -0.3f;

    if (showL) {
        float offx = sinf(t * 41.0f) * 4.0f;
        drawXEye(gfx, (int16_t)(LX + offx), CY, 24, colors.eye, 255);
    }
    if (showR) {
        float offx = sinf(t * 53.0f) * 4.0f;
        drawXEye(gfx, (int16_t)(RX + offx), CY, 24, colors.eye, 255);
    }

    // Occasional horizontal glitch line
    if (g1 > 0.7f) {
        int16_t gy = (int16_t)(CY + sinf(t * 200.0f) * 30.0f);
        gfx.drawLine(0, gy, SCREEN_W, gy, colors.accent, 2, 100);
    }
}

// dead-droop: X eyes that slowly sag downward + slack mouth line
static void render_dead_droop(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sag = ease::in(clamp(t * 1.2f, 0.0f, 1.0f)) * 20.0f;
    int16_t y = (int16_t)(CY + sag);
    uint8_t alpha = (uint8_t)lerp(255.0f, 140.0f, clamp(t, 0.0f, 1.0f));
    drawXEye(gfx, LX, y, 26, colors.eye, alpha);
    drawXEye(gfx, RX, y, 26, colors.eye, alpha);

    // Slack mouth line
    int16_t mx = SCREEN_W / 2;
    int16_t my = (int16_t)(SCREEN_H - 28 + sag * 0.3f);
    gfx.drawLine(mx - 24, my, mx + 24, (int16_t)(my + 4), colors.eye, 4, alpha);
}

const VariantDef DEAD_VARIANTS[] = {
    {"dead-x",      "X eyes",  2800, TONE_NONE, render_dead_x},
    {"dead-fade",   "Fade",    3600, TONE_NONE, render_dead_fade},
    {"dead-glitch", "Glitch",  1800, TONE_NONE, render_dead_glitch},
    {"dead-droop",  "Droop",   3200, TONE_NONE, render_dead_droop},
};

extern const CategoryDef CAT_DEAD = {
    "dead", "Dead", ContentKind::EMOTION, TONE_RED,
    DEAD_VARIANTS, sizeof(DEAD_VARIANTS) / sizeof(DEAD_VARIANTS[0])
};
