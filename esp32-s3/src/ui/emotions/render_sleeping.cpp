#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// sleeping-zzz: closed arc eyes + drifting "z" characters
static void render_sleeping_zzz(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Closed eyes (smile arcs, curving downward)
    int16_t hw = EYE_W / 2;
    int16_t cy = CY + 14;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 14, LX + hw, cy, colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 14, RX + hw, cy, colors.eye, 10);

    // Drifting "z" letters
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t + i * 0.33f, 1.0f);
        uint8_t scale = (uint8_t)(1 + p * 2);
        int16_t x = (int16_t)(RX + 22 + i * 6 + p * 24);
        int16_t y = (int16_t)(CY - 24 - p * 36);
        uint8_t op = (uint8_t)((1.0f - p) * 255.0f);
        gfx.drawChar(x, y, 'z', colors.accent, scale, op);
    }
}

// sleeping-calm: gentle breathing closed eyes
static void render_sleeping_calm(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breathe = sinf(t * TAU) * 1.5f;
    int16_t hw = (EYE_W - 6) / 2;
    int16_t cy = (int16_t)(CY + 16 + breathe);
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 12, LX + hw, cy, colors.eye, 9);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 12, RX + hw, cy, colors.eye, 9);
}

// sleeping-deep: closed eyes + Z letters floating up + snore bubble
static void render_sleeping_deep(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = (EYE_W - 4) / 2;
    int16_t cy = CY + 16;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 14, LX + hw, cy, colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 14, RX + hw, cy, colors.eye, 10);

    // Z letters
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t + i * 0.33f, 1.0f);
        uint8_t scale = (uint8_t)(1 + p * 2);
        int16_t x = (int16_t)(RX + 20 + i * 8 + p * 20);
        int16_t y = (int16_t)(CY - 20 - p * 40);
        uint8_t op = (uint8_t)((1.0f - p) * 220.0f);
        gfx.drawChar(x, y, 'Z', colors.accent, scale, op);
    }

    // Snore bubble from nose area
    float bub = fmodf(t * 0.6f, 1.0f);
    int16_t br = (int16_t)(3.0f + bub * 8.0f);
    uint8_t bop = (uint8_t)((1.0f - bub) * 160.0f);
    gfx.pushAlpha(bop);
    gfx.strokeCircle(SCX + 20, (int16_t)(cy + 20 - bub * 16), br, colors.accent, 2);
    gfx.popAlpha();
}

// sleeping-bubble: closed eyes + snore bubble that grows and pops
static void render_sleeping_bubble(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breathe = sinf(t * TAU * 0.5f) * 1.5f;
    int16_t hw = (EYE_W - 6) / 2;
    int16_t cy = (int16_t)(CY + 16 + breathe);
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 12, LX + hw, cy, colors.eye, 9);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 12, RX + hw, cy, colors.eye, 9);

    // Bubble grows from nose and pops
    float bub = fmodf(t, 1.0f);
    int16_t bx = SCX + 16;
    int16_t by = cy + 16;
    if (bub < 0.8f) {
        float grow = bub / 0.8f;
        int16_t br = (int16_t)(4.0f + grow * 14.0f);
        uint8_t op = (uint8_t)(160.0f + grow * 60.0f);
        gfx.pushAlpha(op);
        gfx.strokeCircle(bx, (int16_t)(by - grow * 6), br, colors.accent, 2);
        gfx.popAlpha();
    } else {
        // Pop: small scattered circles
        float pop = (bub - 0.8f) / 0.2f;
        uint8_t op = (uint8_t)((1.0f - pop) * 200.0f);
        for (int i = 0; i < 4; i++) {
            float a = (float)i * (TAU / 4.0f) + 0.3f;
            int16_t px = (int16_t)(bx + cosf(a) * (8 + pop * 12));
            int16_t py = (int16_t)(by - 10 + sinf(a) * (8 + pop * 12));
            gfx.fillCircle(px, py, 2, colors.accent, op);
        }
    }
}

// --- Category registration ---
const VariantDef SLEEPING_VARIANTS[] = {
    {"sleeping-zzz",    "Zzz drift",  3200, TONE_NONE, render_sleeping_zzz},
    {"sleeping-calm",   "Calm",       4400, TONE_NONE, render_sleeping_calm},
    {"sleeping-deep",   "Deep sleep", 3600, TONE_NONE, render_sleeping_deep},
    {"sleeping-bubble", "Bubble",     3000, TONE_NONE, render_sleeping_bubble},
};

extern const CategoryDef CAT_SLEEPING = {
    "sleeping", "Sleeping", ContentKind::EMOTION, TONE_PURPLE,
    SLEEPING_VARIANTS, sizeof(SLEEPING_VARIANTS) / sizeof(SLEEPING_VARIANTS[0])
};
