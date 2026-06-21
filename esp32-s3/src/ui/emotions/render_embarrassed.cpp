#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// embarrassed-blush: look away + hatched blush patches under each eye
static void render_embarrassed_blush(GfxEngine& gfx, float t, const ColorContext& colors) {
    float side = sinf(t * TAU * 0.6f) * 12.0f - 6.0f;
    int16_t h = (int16_t)(EYE_H * 0.65f);
    int16_t y = CY + 4;
    gfx.drawEye(LX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.fillCircle((int16_t)(LX + side), CY + 12, 9, colors.bg);
    gfx.fillCircle((int16_t)(RX + side), CY + 12, 9, colors.bg);

    int16_t cxs[2] = {LX, RX};
    for (int e = 0; e < 2; e++) {
        int16_t bx = cxs[e] - 22, by = CY + 36;
        for (int i = 0; i < 4; i++) {
            uint8_t op = (i % 2 == 0) ? 255 : 115;
            gfx.drawLine((int16_t)(bx + i * 11), by, (int16_t)(bx + i * 11 - 6),
                         by + 10, colors.accent, 3, op);
        }
    }
}

// embarrassed-hide: peeking eyes behind vertical finger bars
static void render_embarrassed_hide(GfxEngine& gfx, float t, const ColorContext& colors) {
    float peek = (sinf(t * TAU) + 1.0f) / 2.0f;
    int16_t h = (int16_t)lerp(EYE_H * 0.3f, EYE_H * 0.7f, peek);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // hand: vertical bg bars partially covering the eyes
    int16_t fxs[4] = {(int16_t)(LX - 38), (int16_t)(LX + 4), (int16_t)(RX - 14), (int16_t)(RX + 28)};
    for (int i = 0; i < 4; i++)
        gfx.fillRect(fxs[i], STATUS_H + 2, 10, SCREEN_H - STATUS_H - 2, colors.bg);
    int16_t oxs[4] = {(int16_t)(LX - 28), (int16_t)(LX + 14), (int16_t)(RX - 24), (int16_t)(RX + 18)};
    for (int i = 0; i < 4; i++)
        gfx.drawLine(oxs[i], STATUS_H + 4, oxs[i], SCREEN_H - 4, colors.accent, 1, 140);
}

// embarrassed-cringe: hard squint + tight V-brows + gritted teeth
static void render_embarrassed_cringe(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 1.5f;
    gfx.pushTransform();
    gfx.translate(0.0f, wob);
    gfx.drawEye(LX, CY + 4, EYE_W, 14, 6, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, 14, 6, 0, colors.eye);
    // tight V brows
    gfx.drawLine(LX - 30, CY - 32, LX + 26, CY - 14, colors.eye, 6);
    gfx.drawLine(RX + 30, CY - 32, RX - 26, CY - 14, colors.eye, 6);
    // gritted teeth: mouth box + vertical bars
    gfx.strokeRect(SCREEN_W / 2 - 32, SCREEN_H - 28, 64, 10, colors.eye, 2);
    for (int i = 0; i < 6; i++) {
        int16_t x = (int16_t)(SCREEN_W / 2 - 30 + i * 11);
        gfx.drawLine(x, SCREEN_H - 26, x, SCREEN_H - 20, colors.eye, 2);
    }
    gfx.popTransform();
}

// --- Category registration ---
const VariantDef EMBARRASSED_VARIANTS[] = {
    {"embarrassed-blush",  "Blush",  2800, TONE_NONE, render_embarrassed_blush},
    {"embarrassed-hide",   "Hide",   3000, TONE_NONE, render_embarrassed_hide},
    {"embarrassed-cringe", "Cringe", 2600, TONE_NONE, render_embarrassed_cringe},
};

extern const CategoryDef CAT_EMBARRASSED = {
    "embarrassed", "Embarrassed", ContentKind::EMOTION, TONE_ROSE,
    EMBARRASSED_VARIANTS, sizeof(EMBARRASSED_VARIANTS) / sizeof(EMBARRASSED_VARIANTS[0])
};
