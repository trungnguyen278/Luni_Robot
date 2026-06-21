#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// loading-dots: three pulsing circles
static void render_loading_dots(GfxEngine& gfx, float t, const ColorContext& colors) {
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t - i * 0.15f + 1.0f, 1.0f);
        float op = p < 0.4f ? 0.3f + (p / 0.4f) * 0.7f
                            : 1.0f - (p - 0.4f) * 0.8f;
        op = clamp(op, 0.3f, 1.0f);
        float r = 14.0f + fmaxf(0.0f, op - 0.3f) * 6.0f;
        gfx.fillCircle((int16_t)(SCREEN_W / 2 - 56 + i * 56), CY,
                       (int16_t)r, colors.accent, (uint8_t)(op * 255.0f));
    }
}

// loading-bar: small eyes + progress bar at bottom
static void render_loading_bar(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Bar outline
    int16_t bx = 40, by = SCREEN_H - 28;
    int16_t bw = SCREEN_W - 80, bh = 8;
    gfx.strokeRoundedRect(bx, by, bw, bh, 4, colors.accent, 1);

    // Bar fill
    int16_t fw = (int16_t)((float)(bw - 4) * t);
    if (fw > 0) {
        gfx.fillRoundedRect(bx + 2, by + 2, fw, bh - 4, 2, colors.accent);
    }
}

// loading-spinner-big: large spinning arc + dot
static void render_loading_spinner(GfxEngine& gfx, float t, const ColorContext& colors) {
    float ang = t * TAU;
    int16_t cx = SCREEN_W / 2;
    int16_t cy = CY;

    // Draw half-arc
    gfx.drawArc(cx, cy, 38, ang, ang + 3.14159f, colors.accent, 8);
    // Dot at leading end
    float leadAng = ang + 3.14159f;
    int16_t dx = (int16_t)(cx + cosf(leadAng) * 38.0f);
    int16_t dy = (int16_t)(cy + sinf(leadAng) * 38.0f);
    gfx.fillCircle(dx, dy, 6, colors.accent);
}

// loading-rings: normal eyes + concentric expanding ring circles
static void render_loading_rings(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Concentric expanding rings
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.2f + i * 0.33f, 1.0f);
        int16_t r = (int16_t)(8.0f + p * 50.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 160.0f);
        gfx.pushAlpha(op);
        gfx.strokeCircle(SCX, CY, r, colors.accent, 2);
        gfx.popAlpha();
    }
}

// loading-bricks: narrow eyes + a row of 10 bricks lighting up in sequence
static void render_loading_bricks(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.4f);
    gfx.drawEye(LX, CY, EYE_W, h, 8, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 8, 0, colors.eye);

    constexpr int N = 10;
    float total = fmodf(t * (float)N, (float)N);
    for (int i = 0; i < N; i++) {
        uint8_t op = ((float)i < total) ? 255 : 46; // lit vs 0.18 dim
        gfx.fillRoundedRect((int16_t)(24 + i * 28), SCREEN_H - 22, 22, 10, 2,
                            colors.accent, op);
    }
}

const VariantDef LOADING_VARIANTS[] = {
    {"loading-dots",        "Three dots",   1200, TONE_NONE, render_loading_dots},
    {"loading-bar",         "Progress bar", 2400, TONE_NONE, render_loading_bar},
    {"loading-spinner-big", "Big spinner",  1000, TONE_NONE, render_loading_spinner},
    {"loading-rings",       "Rings",        2000, TONE_NONE, render_loading_rings},
    {"loading-bricks",      "Bricks",       1600, TONE_NONE, render_loading_bricks},
};

extern const CategoryDef CAT_LOADING = {
    "loading", "Loading", ContentKind::EMOTION, TONE_CYAN,
    LOADING_VARIANTS, sizeof(LOADING_VARIANTS) / sizeof(LOADING_VARIANTS[0])
};
