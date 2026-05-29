#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// disgusted-yuck: asymmetric narrow eyes (left more squinted), wrinkled brow lines
static void render_disgusted_yuck(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 1.5f) * 1.0f;

    // Left eye more squinted
    int16_t lh = (int16_t)(EYE_H * 0.35f + sh);
    gfx.drawEye(LX, CY, EYE_W, lh, 10, 0, colors.eye);

    // Right eye slightly wider
    int16_t rh = (int16_t)(EYE_H * 0.5f - sh);
    gfx.drawEye(RX, CY, EYE_W, rh, 14, 0, colors.eye);

    // Wrinkle brow lines above nose bridge area
    float wob = sinf(t * TAU * 3.0f) * 1.5f;
    for (int i = 0; i < 3; i++) {
        int16_t y = (int16_t)(CY - 52 + i * 5 + wob);
        int16_t x0 = (int16_t)(SCX - 18 + i * 3);
        int16_t x1 = (int16_t)(SCX + 18 - i * 3);
        gfx.drawLine(x0, y, x1, (int16_t)(y + 2), colors.eye, 2, 180);
    }
}

// disgusted-gag: eyes close slightly with push-forward motion (translate y), throat indicator
static void render_disgusted_gag(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Gag reflex cycle
    float cyc = fmodf(t * 0.7f, 1.0f);
    float gag = 0.0f;
    if (cyc > 0.3f && cyc < 0.6f) {
        gag = sinf((cyc - 0.3f) / 0.3f * PI);
    }

    float dy = gag * 8.0f;
    float squeeze = 1.0f - gag * 0.3f;
    int16_t h = (int16_t)(EYE_H * 0.6f * squeeze);

    gfx.pushTransform();
    gfx.translate(0, dy);
    gfx.drawEye(LX, CY, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 14, 0, colors.eye);
    gfx.popTransform();

    // Throat indicator: small circle that pulses at center-bottom
    int16_t ty = (int16_t)(SCREEN_H - 28 + dy * 0.5f);
    uint8_t top = (uint8_t)(gag * 200.0f);
    if (top > 20) {
        gfx.fillCircle(SCX, ty, (int16_t)(4 + gag * 3), colors.accent, top);
        gfx.fillCircle(SCX, (int16_t)(ty - 8), (int16_t)(3 + gag * 2), colors.accent,
                       (uint8_t)(top * 0.6f));
    }
}

// disgusted-sour: both eyes very narrow with bg lid overlap, mouth line (tiny downward arc)
static void render_disgusted_sour(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sq = sinf(t * TAU * 1.2f) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.38f + sq);
    gfx.drawEye(LX, CY, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 10, 0, colors.eye);

    // Bg lid overlap: rounded rects covering top portion of each eye
    int16_t lidH = (int16_t)(12.0f - sq * 0.5f);
    gfx.fillRoundedRect(LX - EYE_W / 2, CY - h / 2 - 4, EYE_W, lidH, 6,
                        colors.bg);
    gfx.fillRoundedRect(RX - EYE_W / 2, CY - h / 2 - 4, EYE_W, lidH, 6,
                        colors.bg);

    // Tiny downward mouth arc
    float mwob = sinf(t * TAU * 0.8f) * 2.0f;
    int16_t mx = SCX;
    int16_t my = (int16_t)(SCREEN_H - 32 + mwob);
    gfx.pushAlpha(200);
    gfx.drawQuadBezier(mx - 22, my, mx, my + 10, mx + 22, my, colors.eye, 4);
    gfx.popAlpha();
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
