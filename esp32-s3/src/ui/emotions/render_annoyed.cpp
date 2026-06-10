#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// annoyed-flat: deadpan flat eye-bars + angled-down brows + flat mouth
static void render_annoyed_flat(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t tw = (t > 0.45f && t < 0.5f) ? 3 : 0;
    gfx.pushTransform();
    gfx.translate(0.0f, (float)tw);
    int16_t w = (int16_t)(EYE_W * 1.05f);
    gfx.drawEye(LX, CY, w, 10, 5, 0, colors.eye);
    gfx.drawEye(RX, CY, w, 10, 5, 0, colors.eye);
    gfx.drawLine(LX - 32, CY - 28, LX + 28, CY - 22, colors.eye, 5);
    gfx.drawLine(RX + 32, CY - 28, RX - 28, CY - 22, colors.eye, 5);
    gfx.drawLine(SCREEN_W / 2 - 18, SCREEN_H - 26, SCREEN_W / 2 + 18, SCREEN_H - 26,
                 colors.eye, 4);
    gfx.popTransform();
}

// annoyed-twitch: left eye twitches periodically; right stays steady half-closed
static void render_annoyed_twitch(GfxEngine& gfx, float t, const ColorContext& colors) {
    bool burst = (t > 0.55f && t < 0.62f);
    float tw = burst ? sinf((t - 0.55f) / 0.07f * PI) * 8.0f : 0.0f;

    int16_t lh = burst ? 6 : (int16_t)(EYE_H * 0.45f);
    gfx.drawEye((int16_t)(LX + tw * 0.3f), CY + 6, EYE_W, lh, 10, 0, colors.eye);
    gfx.drawEye(RX, CY + 6, EYE_W, (int16_t)(EYE_H * 0.45f), 10, 0, colors.eye);

    // Angled-in brows (left lifts with the twitch)
    gfx.drawLine(LX - 32, (int16_t)(CY - 30 - tw), LX + 28, CY - 20, colors.eye, 6);
    gfx.drawLine(RX + 32, CY - 30, RX - 28, CY - 20, colors.eye, 6);
}

// annoyed-sigh-side: eyes look to the side; sigh puff drifts up-right
static void render_annoyed_sigh_side(GfxEngine& gfx, float t, const ColorContext& colors) {
    float side = sinf(t * TAU * 0.6f) * 8.0f + 6.0f;
    float phase = fmodf(t, 1.0f);
    int16_t h = (int16_t)(EYE_H * 0.5f);

    gfx.drawEye(LX, CY + 6, EYE_W, h, 12, 0, colors.eye);
    gfx.drawEye(RX, CY + 6, EYE_W, h, 12, 0, colors.eye);

    // Pupils glance to the side
    gfx.fillCircle((int16_t)(LX + side), CY + 10, 8, colors.bg);
    gfx.fillCircle((int16_t)(RX + side), CY + 10, 8, colors.bg);

    // Sigh puff drifting up-right
    if (phase > 0.35f) {
        float p = phase - 0.35f;
        int16_t px = (int16_t)(SCX + 20 + p * 40.0f);
        int16_t py = (int16_t)(SCREEN_H - 28 - p * 24.0f);
        int16_t pr = (int16_t)(4.0f + p * 4.0f);
        float op = 1.0f - p * 1.5f;
        if (op > 0.0f) {
            gfx.drawArc(px, py, pr, 0.0f, TAU, colors.accent, 2,
                        (uint8_t)(op * 255.0f));
        }
    }
}

// --- Category registration ---
const VariantDef ANNOYED_VARIANTS[] = {
    {"annoyed-flat",      "Flat",      2600, TONE_NONE, render_annoyed_flat},
    {"annoyed-twitch",    "Twitch",    2800, TONE_NONE, render_annoyed_twitch},
    {"annoyed-sigh-side", "Sigh side", 3000, TONE_NONE, render_annoyed_sigh_side},
};

extern const CategoryDef CAT_ANNOYED = {
    "annoyed", "Annoyed", ContentKind::EMOTION, TONE_ORANGE,
    ANNOYED_VARIANTS, sizeof(ANNOYED_VARIANTS) / sizeof(ANNOYED_VARIANTS[0])
};
