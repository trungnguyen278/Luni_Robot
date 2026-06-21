#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// disgusted-yuck: asymmetric squint + angled brows + tongue sticking out
static void render_disgusted_yuck(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 2.0f) * 1.5f;
    gfx.drawEye(LX, (int16_t)(CY + wob), EYE_W, 10, 6, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, (int16_t)(EYE_H * 0.45f), 14, 0, colors.eye);

    // angled disgust brows
    gfx.drawLine(LX - 32, CY - 26, LX + 28, CY - 38, colors.eye, 6);
    gfx.drawLine(RX - 26, CY - 34, RX + 30, CY - 28, colors.eye, 6);

    // tongue out below (rounded blob)
    int16_t mx = SCREEN_W / 2;
    gfx.fillRoundedRect(mx - 14, (int16_t)(SCREEN_H - 28 + wob), 28, 16, 7,
                        colors.accent, 217);
}

// disgusted-gag: eyes go wide on gag, lower lid pushes up, shrunken pupils, throat puff
static void render_disgusted_gag(GfxEngine& gfx, float t, const ColorContext& colors) {
    float gag = pulse(t, 0.4f, 0.2f);
    float sz = 1.0f + gag * 0.18f;
    int16_t w = (int16_t)(EYE_W * sz);
    int16_t h = (int16_t)(EYE_H * sz);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Lower lid pushes up (bg mask over bottom of each eye)
    int16_t lift = (int16_t)(14.0f + gag * 10.0f);
    int16_t cxs[2] = {LX, RX};
    for (int e = 0; e < 2; e++) {
        gfx.fillRect((int16_t)(cxs[e] - EYE_W / 2 - 6), (int16_t)(CY + EYE_H / 2 - lift),
                     EYE_W + 12, (int16_t)(lift + 6), colors.bg);
    }

    // Shrunken pupils
    gfx.fillCircle(LX, CY - 6, 6, colors.bg);
    gfx.fillCircle(RX, CY - 6, 6, colors.bg);

    // Gag puff outline + small arc above
    int16_t pr = (int16_t)(11.0f + gag * 8.0f);
    gfx.drawArc(SCX, SCREEN_H - 22, pr, 0.0f, TAU, colors.accent, 3, 204);
    gfx.drawQuadBezier(SCX - 6, SCREEN_H - 24, SCX, SCREEN_H - 30, SCX + 6, SCREEN_H - 24,
                       colors.accent, 2);
}

// disgusted-sour: lemon-sour tight squint, corner creases, puckered mouth
static void render_disgusted_sour(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pucker = 0.85f + sinf(t * TAU) * 0.15f;
    gfx.drawEye(LX, CY, EYE_W, 14, 6, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, 14, 6, 0, colors.eye);

    // Squint diagonal creases at outer corners
    gfx.drawLine(LX - 36, CY - 6, LX - 22, CY - 12, colors.accent, 3, 179);
    gfx.drawLine(LX - 36, CY + 6, LX - 22, CY + 12, colors.accent, 3, 179);
    gfx.drawLine(RX + 36, CY - 6, RX + 22, CY - 12, colors.accent, 3, 179);
    gfx.drawLine(RX + 36, CY + 6, RX + 22, CY + 12, colors.accent, 3, 179);

    // Puckered mouth: outline circle + center dot
    gfx.strokeCircle(SCX, SCREEN_H - 28, (int16_t)(6.0f * pucker), colors.eye, 4);
    gfx.fillCircle(SCX, SCREEN_H - 28, 2, colors.eye);
}

// --- Category registration ---
const VariantDef DISGUSTED_VARIANTS[] = {
    {"disgusted-yuck", "Yuck", 2400, TONE_NONE, render_disgusted_yuck},
    {"disgusted-gag",  "Gag",  2600, TONE_NONE, render_disgusted_gag},
    {"disgusted-sour", "Sour", 2800, TONE_NONE, render_disgusted_sour},
};

extern const CategoryDef CAT_DISGUSTED = {
    "disgusted", "Disgusted", ContentKind::EMOTION, TONE_GREEN,
    DISGUSTED_VARIANTS, sizeof(DISGUSTED_VARIANTS) / sizeof(DISGUSTED_VARIANTS[0])
};
