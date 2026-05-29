#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// bored-half: very narrow eyes that drift side to side + flat mouth
static void render_bored_half(GfxEngine& gfx, float t, const ColorContext& colors) {
    float look = sinf(t * TAU) * 10.0f;
    int16_t h = (int16_t)(EYE_H * 0.35f);
    gfx.drawEye((int16_t)(LX + look), CY + 14, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + look), CY + 14, EYE_W, h, 14, 0, colors.eye);

    // Flat mouth line
    gfx.fillRoundedRect(SCREEN_W / 2 - 20, SCREEN_H - 30, 40, 4, 2,
                        colors.eye, 153); // 0.6 opacity
}

// bored-sideways: half-height eyes with pupils looking to one side
static void render_bored_sideways(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.6f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    int16_t slide = sinf(t * TAU) > 0.0f ? 18 : -18;
    gfx.fillCircle(LX + slide, CY, 14, colors.bg);
    gfx.fillCircle(RX + slide, CY, 14, colors.bg);
}

// bored-eye-roll: pupils roll in circles inside normal-height eyes
static void render_bored_eye_roll(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    float a = t * TAU;
    float pr = 12.0f;
    int16_t px = (int16_t)(cosf(a) * pr);
    int16_t py = (int16_t)(sinf(a) * pr * 0.6f);
    gfx.fillCircle(LX + px, CY + py, 12, colors.bg);
    gfx.fillCircle(RX + px, CY + py, 12, colors.bg);
}

// bored-yawn-small: eyes narrow slowly + small mouth circle at bottom
static void render_bored_yawn_small(GfxEngine& gfx, float t, const ColorContext& colors) {
    float yawn = pulse(t, 0.5f, 0.25f);
    int16_t h = (int16_t)lerp(EYE_H * 0.5f, EYE_H * 0.2f, yawn);
    gfx.drawEye(LX, CY + 10, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, CY + 10, EYE_W, h, 14, 0, colors.eye);

    // Small yawn mouth
    if (yawn > 0.05f) {
        int16_t mrx = (int16_t)(6.0f + yawn * 4.0f);
        int16_t mry = (int16_t)(4.0f + yawn * 8.0f);
        gfx.fillEllipse(SCX, SCREEN_H - 32, mrx, mry, colors.eye);
        gfx.fillEllipse(SCX, SCREEN_H - 32, (int16_t)(mrx - 3), (int16_t)(mry - 3), colors.bg);
    }
}

const VariantDef BORED_VARIANTS[] = {
    {"bored-half",       "Half lids",      4200, TONE_NONE, render_bored_half},
    {"bored-sideways",   "Sideways stare", 3800, TONE_NONE, render_bored_sideways},
    {"bored-eye-roll",   "Eye roll",       2400, TONE_NONE, render_bored_eye_roll},
    {"bored-yawn-small", "Small yawn",     3000, TONE_NONE, render_bored_yawn_small},
};

extern const CategoryDef CAT_BORED = {
    "bored", "Bored", ContentKind::EMOTION, TONE_CYAN,
    BORED_VARIANTS, sizeof(BORED_VARIANTS) / sizeof(BORED_VARIANTS[0])
};
