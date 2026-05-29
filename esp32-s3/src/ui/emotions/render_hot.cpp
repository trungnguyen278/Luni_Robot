#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_hot_sweat(GfxEngine& gfx, float t, const ColorContext& colors) {
    float droop = 2.0f + sinf(t * TAU * 0.6f) * 1.0f;
    int16_t y = (int16_t)(CY + droop);
    int16_t h = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Sweat beads
    int16_t xpos[] = {(int16_t)(LX - 24), SCX, (int16_t)(RX + 24)};
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.2f + i * 0.33f, 1.0f);
        int16_t sy = (int16_t)(STATUS_H + 8 + p * 80.0f);
        uint8_t op = (uint8_t)((1.0f - p * 0.3f) * 255);
        gfx.fillCircle(xpos[i], (int16_t)(sy - 5), 5, colors.accent, op);
        gfx.fillTriangle((int16_t)(xpos[i] - 5), (int16_t)(sy - 5),
                         (int16_t)(xpos[i] + 5), (int16_t)(sy - 5),
                         xpos[i], (int16_t)(sy + 3), colors.accent, op);
    }

    // Heat lines
    for (int i = 0; i < 3; i++) {
        float wob = sinf(t * TAU * 2.0f + i) * 4.0f;
        int16_t hx = (int16_t)(SCREEN_W - 20 - i * 8);
        for (int j = 0; j < 4; j++) {
            float frac = j / 3.0f;
            int16_t y0 = (int16_t)(CY - 30 + j * 16);
            int16_t y1 = (int16_t)(CY - 30 + (j + 1) * 16);
            float w0 = (j % 2 == 0) ? wob : -wob;
            float w1 = ((j + 1) % 2 == 0) ? wob : -wob;
            gfx.drawLine((int16_t)(hx + w0), y0, (int16_t)(hx + w1), y1,
                         colors.accent, 2, 140);
        }
    }
}

static void render_hot_fan(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.7f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Hand-fan swinging
    float swing = sinf(t * TAU * 3.0f) * 24.0f;
    gfx.pushTransform();
    gfx.translate((float)(SCREEN_W - 36), (float)(CY + 8));
    gfx.rotate(swing, 0, 0);
    gfx.fillTriangle(0, 0, -22, -14, -22, 14, colors.accent);
    gfx.drawLine(0, 0, -22, -14, colors.eye, 1, 128);
    gfx.drawLine(0, 0, -26, 0, colors.eye, 1, 128);
    gfx.drawLine(0, 0, -22, 14, colors.eye, 1, 128);
    gfx.popTransform();

    // Wind lines
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.0f + i * 0.3f, 1.0f);
        int16_t lx1 = (int16_t)(SCREEN_W - 70 - p * 24);
        int16_t ly = (int16_t)(CY - 16 + i * 16);
        uint8_t op = (uint8_t)((1.0f - p) * 255);
        gfx.drawLine(lx1, ly, (int16_t)(lx1 - 20), ly, colors.accent, 2, op);
    }
}

static void render_hot_melt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float melt = (sinf(t * TAU - PI / 2.0f) + 1.0f) / 2.0f;
    float dropH = lerp(0.0f, 22.0f, melt);
    int16_t h = (int16_t)(EYE_H * (0.8f - melt * 0.15f));

    int16_t eyes[] = {LX, RX};
    for (int i = 0; i < 2; i++) {
        gfx.drawEye(eyes[i], CY, EYE_W, h, EYE_RX, 0, colors.eye);
        // Drips below eyes
        int16_t dy = (int16_t)(CY + EYE_H / 2);
        gfx.fillEllipse((int16_t)(eyes[i] - 14), (int16_t)(dy + dropH * 0.4f),
                        6, (int16_t)(dropH * 0.5f), colors.eye);
        gfx.fillEllipse((int16_t)(eyes[i] + 14), (int16_t)(dy + dropH * 0.7f),
                        5, (int16_t)(dropH * 0.6f), colors.eye);
        gfx.fillCircle((int16_t)(eyes[i] - 14), (int16_t)(dy + dropH * 0.8f),
                       (int16_t)(4 + melt * 2), colors.eye);
        gfx.fillCircle((int16_t)(eyes[i] + 14), (int16_t)(dy + dropH * 1.2f),
                       (int16_t)(3 + melt * 2), colors.eye);
    }

    // Puddle at bottom
    int16_t prx = (int16_t)lerp(20.0f, 70.0f, melt);
    int16_t pry = (int16_t)lerp(2.0f, 6.0f, melt);
    gfx.fillEllipse(SCX, SCREEN_H - 14, prx, pry, colors.accent, 153);
}

const VariantDef HOT_VARIANTS[] = {
    {"hot-sweat", "Sweat drops", 2200, TONE_NONE, render_hot_sweat},
    {"hot-fan",   "Fan self",    1400, TONE_NONE, render_hot_fan},
    {"hot-melt",  "Melting",     3000, TONE_NONE, render_hot_melt},
};

extern const CategoryDef CAT_HOT = {
    "hot", "Hot", ContentKind::EMOTION, TONE_RED,
    HOT_VARIANTS, sizeof(HOT_VARIANTS) / sizeof(HOT_VARIANTS[0])
};
