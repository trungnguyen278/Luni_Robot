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

// surprised-flash: rapid eye size pulse + flash lines radiating out
static void render_surprised_flash(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pulse_v = 1.0f + sinf(t * TAU * 4.0f) * 0.15f;
    int16_t r = (int16_t)((float)(EYE_W / 2) * 1.2f * pulse_v);
    gfx.fillCircle(LX, CY, r, colors.eye);
    gfx.fillCircle(RX, CY, r, colors.eye);

    // Flash lines radiating from center
    for (int i = 0; i < 8; i++) {
        float a = (float)i * (TAU / 8.0f) + t * TAU * 2.0f;
        float inner = 55.0f;
        float outer = 55.0f + sinf(t * TAU * 3.0f + (float)i) * 12.0f + 12.0f;
        int16_t x0 = (int16_t)(SCX + cosf(a) * inner);
        int16_t y0 = (int16_t)(CY + sinf(a) * inner);
        int16_t x1 = (int16_t)(SCX + cosf(a) * outer);
        int16_t y1 = (int16_t)(CY + sinf(a) * outer);
        gfx.drawLine(x0, y0, x1, y1, colors.accent, 3, 200);
    }
}

// surprised-mild: less extreme pop, raised brow lines
static void render_surprised_mild(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = 1.0f + sinf(t * TAU * 0.5f) * 0.02f;
    int16_t w = (int16_t)((float)EYE_W * 1.02f * s);
    int16_t h = (int16_t)((float)EYE_H * 1.02f * s);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Raised brow lines
    float lift = sinf(t * TAU * 0.5f) * 3.0f;
    int16_t by = (int16_t)(CY - 56 + lift);
    gfx.drawLine(LX - 26, by, LX + 26, (int16_t)(by - 4), colors.eye, 5);
    gfx.drawLine(RX - 26, (int16_t)(by - 4), RX + 26, by, colors.eye, 5);
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
