#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void render_playful_blep(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 2.0f;
    float tongueOut = pulse(t, 0.5f, 0.35f);
    int16_t h = (int16_t)(EYE_H * 0.85f);
    int16_t y = (int16_t)(CY - 4 + wob);
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);

    // Tongue
    if (tongueOut > 0.05f) {
        int16_t ty = (int16_t)(SCREEN_H - 24 + tongueOut * 16.0f);
        int16_t trx = 12, tryy = (int16_t)(9 + tongueOut * 4.0f);
        gfx.fillEllipse(SCX, ty, trx, tryy, colors.accent);
        gfx.drawLine(SCX, (int16_t)(ty - tryy + 4), SCX,
                     (int16_t)(ty + tryy - 4), colors.bg, 1, 153);
    }
}

static void render_playful_tease(GfxEngine& gfx, float t, const ColorContext& colors) {
    float close = pulse(t, 0.35f, 0.18f);
    float stick = pulse(t, 0.5f, 0.3f);

    if (close > 0.5f) {
        // Winking left eye
        int16_t hw = (EYE_W - 6) / 2;
        gfx.drawQuadBezier(LX - hw, CY + 6, LX, CY + 6 + 18, LX + hw, CY + 6,
                           colors.eye, 12);
    } else {
        int16_t lh = (int16_t)(EYE_H * (1.0f - close * 0.6f));
        gfx.drawEye(LX, CY, EYE_W, lh, EYE_RX, 0, colors.eye);
    }
    int16_t rh = (int16_t)(EYE_H * 0.95f);
    gfx.drawEye(RX, CY, EYE_W, rh, EYE_RX, 0, colors.eye);

    // Tongue stick out
    if (stick > 0.05f) {
        int16_t tx = SCX + 8;
        int16_t ty = (int16_t)(SCREEN_H - 26 + stick * 8.0f);
        gfx.fillEllipse(tx, ty, 10, (int16_t)(6 + stick * 3.0f), colors.accent);
    }
}

static void render_playful_boop(GfxEngine& gfx, float t, const ColorContext& colors) {
    float hit = pulse(t, 0.5f, 0.12f);
    float sq = 1.0f - hit * 0.18f;
    int16_t w = (int16_t)(EYE_W * sq);
    int16_t h = (int16_t)(EYE_H * sq);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Boop rings
    if (hit > 0.05f) {
        for (int i = 0; i < 3; i++) {
            int16_t r = (int16_t)(6 + i * 12 * hit);
            uint8_t op = (uint8_t)((1.0f - i * 0.3f) * (1.0f - hit * 0.5f) * 255);
            gfx.strokeCircle(SCX, CY + 2, r, colors.accent, 2);
        }
    }

    // Smile
    gfx.drawQuadBezier(SCX - 25, SCREEN_H - 30, SCX, SCREEN_H - 20,
                       SCX + 25, SCREEN_H - 30, colors.eye, 5);
}

const VariantDef PLAYFUL_VARIANTS[] = {
    {"playful-blep",  "Tongue blep",  2000, TONE_NONE, render_playful_blep},
    {"playful-tease", "Wink + stick", 2400, TONE_NONE, render_playful_tease},
    {"playful-boop",  "Boop",         1800, TONE_NONE, render_playful_boop},
};

extern const CategoryDef CAT_PLAYFUL = {
    "playful", "Playful", ContentKind::EMOTION, TONE_ROSE,
    PLAYFUL_VARIANTS, sizeof(PLAYFUL_VARIANTS) / sizeof(PLAYFUL_VARIANTS[0])
};
