#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// surprised-pop: eyes pop to large circles + mouth O
static void render_surprised_pop(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s;
    if (t < 0.12f) {
        s = ease::out(t / 0.12f) * 1.35f;
    } else if (t < 0.7f) {
        s = 1.35f - sinf((t - 0.12f) * TAU * 1.5f) * 0.04f;
    } else {
        s = lerp(1.35f, 1.0f, ease::inOut((t - 0.7f) / 0.3f));
    }
    int16_t r = (int16_t)((float)(EYE_W / 2) * s);
    gfx.fillCircle(LX, CY, r, colors.eye);
    gfx.fillCircle(RX, CY, r, colors.eye);

    // Mouth O
    int16_t mr = (int16_t)(6.0f + s * 2.0f);
    gfx.strokeCircle(SCREEN_W / 2, SCREEN_H - 32, mr, colors.eye, 4);
}

// surprised-exclaim: big round eyes with highlight glints + exclamation mark
static void render_surprised_exclaim(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = t < 0.15f ? ease::out(t / 0.15f) : 1.0f;
    int16_t r = (int16_t)((float)(EYE_W / 2) * 1.1f * s);
    gfx.fillCircle(LX, CY, r, colors.eye);
    gfx.fillCircle(RX, CY, r, colors.eye);
    // Highlight glints
    gfx.fillCircle(LX - 8, CY - 14, 5, colors.bg);
    gfx.fillCircle(RX - 8, CY - 14, 5, colors.bg);

    // Exclamation mark (appears t=0.1..0.55)
    if (t > 0.1f && t < 0.55f) {
        float op = 1.0f - fabsf((t - 0.32f) / 0.25f);
        uint8_t alpha = (uint8_t)(clamp(op, 0.0f, 1.0f) * 255.0f);
        int16_t mx = SCREEN_W / 2;
        gfx.fillRoundedRect(mx - 4, SCREEN_H - 60, 8, 22, 2, colors.accent, alpha);
        gfx.fillCircle(mx, SCREEN_H - 26, 4, colors.accent, alpha);
    }
}

// surprised-flash: single pop pulse + 5 flash lines bursting up top
static void render_surprised_flash(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = pulse(t, 0.2f, 0.18f);
    int16_t r = (int16_t)((float)(EYE_W / 2) * (1.0f + s * 0.4f));
    gfx.fillCircle(LX, CY, r, colors.eye);
    gfx.fillCircle(RX, CY, r, colors.eye);

    uint8_t a = (uint8_t)(fmaxf(0.0f, 1.0f - (t - 0.1f) / 0.35f) * 255.0f);
    for (int i = -2; i <= 2; i++) {
        gfx.drawLine((int16_t)(SCREEN_W / 2 + i * 16), STATUS_H + 8,
                     (int16_t)(SCREEN_W / 2 + i * 24), STATUS_H - 4,
                     colors.accent, 3, a);
    }
}

// surprised-mild: one gentle grow-then-shrink pop with glints
static void render_surprised_mild(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = (t < 0.35f) ? ease::out(t / 0.35f)
                          : 1.0f - ease::in((t - 0.35f) / 0.65f);
    int16_t sz = (int16_t)((float)(EYE_W / 2) * (1.0f + s * 0.22f));
    gfx.fillCircle(LX, CY, sz, colors.eye);
    gfx.fillCircle(RX, CY, sz, colors.eye);
    gfx.fillCircle(LX - 6, CY - 10, 5, colors.bg);
    gfx.fillCircle(RX - 6, CY - 10, 5, colors.bg);
}

// --- Category registration ---
const VariantDef SURPRISED_VARIANTS[] = {
    {"surprised-pop",     "Pop",     2200, TONE_NONE, render_surprised_pop},
    {"surprised-exclaim", "Exclaim", 2400, TONE_NONE, render_surprised_exclaim},
    {"surprised-flash",   "Flash",   1400, TONE_NONE, render_surprised_flash},
    {"surprised-mild",    "Mild",    2400, TONE_NONE, render_surprised_mild},
};

extern const CategoryDef CAT_SURPRISED = {
    "surprised", "Surprised", ContentKind::EMOTION, TONE_WHITE,
    SURPRISED_VARIANTS, sizeof(SURPRISED_VARIANTS) / sizeof(SURPRISED_VARIANTS[0])
};
