#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// annoyed-flat: very flat narrow eyes, expressionless, occasional micro-twitch
static void render_annoyed_flat(GfxEngine& gfx, float t, const ColorContext& colors) {
    float twitch = 0.0f;
    float tp = fmodf(t * 0.8f, 1.0f);
    if (tp > 0.7f && tp < 0.75f) {
        twitch = sinf((tp - 0.7f) / 0.05f * PI) * 3.0f;
    }

    int16_t h = (int16_t)(EYE_H * 0.35f);
    gfx.drawEye(LX, (int16_t)(CY + 4), EYE_W, (int16_t)(h + twitch), 8, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + 4), EYE_W, h, 8, 0, colors.eye);
}

// annoyed-twitch: normal-ish eyes but right eye twitches with brief height pulse
static void render_annoyed_twitch(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t lh = (int16_t)(EYE_H * 0.65f);
    gfx.drawEye(LX, CY, EYE_W, lh, EYE_RX, 0, colors.eye);

    // Right eye twitch: periodic rapid pulse
    float cyc = fmodf(t * 1.2f, 1.0f);
    float tw = 0.0f;
    if (cyc > 0.4f && cyc < 0.55f) {
        tw = sinf((cyc - 0.4f) / 0.15f * PI) * 0.35f;
    }
    if (cyc > 0.6f && cyc < 0.7f) {
        tw = sinf((cyc - 0.6f) / 0.1f * PI) * 0.25f;
    }

    int16_t rh = (int16_t)(EYE_H * (0.65f - tw));
    gfx.drawEye(RX, CY, EYE_W, rh, EYE_RX, 0, colors.eye);

    // Subtle angry brow on right side
    float bsh = sinf(t * TAU * 5.0f) * tw * 4.0f;
    if (tw > 0.05f) {
        gfx.drawLine(RX - 30, (int16_t)(CY - 44 + bsh), RX + 30, CY - 34,
                     colors.eye, 4);
    }
}

// annoyed-sigh-side: eyes roll to one side slowly then back, slight droop
static void render_annoyed_sigh_side(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Smooth side-roll: ease in-out
    float phase = fmodf(t * 0.6f, 1.0f);
    float roll;
    if (phase < 0.4f) {
        roll = ease::inOut(phase / 0.4f);
    } else if (phase < 0.7f) {
        roll = 1.0f;
    } else {
        roll = 1.0f - ease::inOut((phase - 0.7f) / 0.3f);
    }
    float dx = roll * 18.0f;

    float droop = 3.0f + sinf(t * TAU * 0.4f) * 1.5f;
    int16_t h = (int16_t)(EYE_H * 0.55f);

    gfx.drawEye((int16_t)(LX + dx), (int16_t)(CY + droop), EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + dx), (int16_t)(CY + droop), EYE_W, h, 14, 0, colors.eye);

    // Droopy lids: bg rectangles over top of eyes
    int16_t lidH = (int16_t)(8.0f + droop);
    gfx.fillRect((int16_t)(LX + dx - EYE_W / 2), (int16_t)(CY + droop - h / 2 - 2),
                 EYE_W, lidH, colors.bg);
    gfx.fillRect((int16_t)(RX + dx - EYE_W / 2), (int16_t)(CY + droop - h / 2 - 2),
                 EYE_W, lidH, colors.bg);
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
