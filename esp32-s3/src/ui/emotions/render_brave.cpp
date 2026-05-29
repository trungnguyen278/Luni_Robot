#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_brave_stance(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breath = sinf(t * TAU * 0.7f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.5f);
    int16_t y = (int16_t)(CY + 4 + breath);
    gfx.drawEye(LX, y, EYE_W, h, 8, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, 8, 0, colors.eye);

    // V-brows
    gfx.drawLine(LX - 38, CY - 44, LX + 28, CY - 26, colors.eye, 9);
    gfx.drawLine(RX + 38, CY - 44, RX - 28, CY - 26, colors.eye, 9);

    // Small flame
    gfx.pushTransform();
    gfx.translate((float)SCX, (float)(STATUS_H + 24));
    gfx.scale(1.0f + breath * 0.05f, 1.0f + breath * 0.05f);
    gfx.fillTriangle(-4, 8, -8, -4, 0, -10, colors.accent);
    gfx.fillTriangle(0, -10, 4, -6, 4, 8, colors.accent);
    gfx.fillTriangle(-4, 8, 4, 8, 0, -10, colors.accent);
    gfx.popTransform();
}

static void render_brave_shield(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pop = t < 0.2f ? ease::out(t / 0.2f) : 1.0f;

    // Shield
    gfx.pushTransform();
    gfx.translate((float)SCX, (float)(CY + 8));
    gfx.scale(pop, pop);
    // Shield body (approximated with triangles)
    gfx.fillTriangle(-40, -34, 0, -42, 40, -34, colors.eye);
    gfx.fillRect(-40, -34, 80, 42, colors.eye);
    gfx.fillTriangle(-36, 8, 0, 42, 36, 8, colors.eye);
    // Arrow emblem
    gfx.fillRect(-6, -22, 12, 22, colors.bg, 178);
    gfx.fillTriangle(-14, 0, 0, 16, 14, 0, colors.bg, 178);
    gfx.popTransform();

    // Eyes peeking above shield
    int16_t ey = CY - 32;
    int16_t eh = (int16_t)(EYE_H * 0.35f);
    gfx.drawEye(LX, ey, EYE_W, eh, 10, 0, colors.eye);
    gfx.drawEye(RX, ey, EYE_W, eh, 10, 0, colors.eye);
}

static void render_brave_charge(GfxEngine& gfx, float t, const ColorContext& colors) {
    float step = sinf(t * TAU * 2.0f) * 6.0f;

    gfx.pushTransform();
    gfx.translate(step, 0);
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, CY, EYE_W, h, 10, -8, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 10, 8, colors.eye);
    gfx.popTransform();

    // Speed lines
    for (int i = 0; i < 4; i++) {
        float p = fmodf(t * 3.0f + i * 0.25f, 1.0f);
        int16_t lx = (int16_t)(2 + p * 28);
        int16_t ly = (int16_t)(STATUS_H + 30 + i * 36);
        uint8_t op = (uint8_t)((1.0f - p) * 255);
        gfx.drawLine(lx, ly, (int16_t)(lx + 18), ly, colors.accent, 2, op);
    }

    // Forward arrow
    int16_t ax = SCREEN_W - 12;
    gfx.drawLine(SCREEN_W - 28, CY - 10, ax, CY, colors.accent, 4);
    gfx.drawLine(ax, CY, SCREEN_W - 28, CY + 10, colors.accent, 4);
    gfx.drawLine(ax, CY, SCREEN_W - 40, CY, colors.accent, 4);
}

const VariantDef BRAVE_VARIANTS[] = {
    {"brave-stance", "Fierce stance", 2400, TONE_NONE, render_brave_stance},
    {"brave-shield", "Shield up",     2200, TONE_NONE, render_brave_shield},
    {"brave-charge", "Charge",        1200, TONE_NONE, render_brave_charge},
};

extern const CategoryDef CAT_BRAVE = {
    "brave", "Brave", ContentKind::EMOTION, TONE_RED,
    BRAVE_VARIANTS, sizeof(BRAVE_VARIANTS) / sizeof(BRAVE_VARIANTS[0])
};
