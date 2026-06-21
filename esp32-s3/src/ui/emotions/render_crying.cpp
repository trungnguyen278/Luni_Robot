#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// crying-tears: closed arc eyes + tear drops falling from outer corners
static void render_crying_tears(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Closed arc eyes
    int16_t hw = EYE_W / 2;
    gfx.drawQuadBezier(LX - hw, CY + 12, LX, CY + 12 + 22, LX + hw, CY + 12,
                       colors.eye, 12);
    gfx.drawQuadBezier(RX - hw, CY + 12, RX, CY + 12 + 22, RX + hw, CY + 12,
                       colors.eye, 12);

    // Tear drops from outer eye corners
    int16_t tearX[2] = {(int16_t)(LX - 32), (int16_t)(RX + 32)};
    for (int side = 0; side < 2; side++) {
        for (int i = 0; i < 2; i++) {
            float p = fmodf(t * 1.5f + i * 0.5f + side * 0.25f, 1.0f);
            int16_t yPos = (int16_t)(CY + 26 + p * 80);
            int16_t len = (int16_t)lerp(6.0f, 16.0f, sinf(p * 3.14159f));
            uint8_t op = (uint8_t)((1.0f - p * 0.5f) * 255.0f);
            gfx.fillEllipse(tearX[side], yPos, 5, len, colors.accent, op);
        }
    }
}

// crying-waterfall: closed eyes + heavy multi-stream tears
static void render_crying_waterfall(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = EYE_W / 2;
    gfx.drawQuadBezier(LX - hw, CY, LX, CY + 26, LX + hw, CY, colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, CY, RX, CY + 26, RX + hw, CY, colors.eye, 10);

    for (int i = 0; i < 6; i++) {
        int16_t x = (int16_t)(LX - 20 + i * 32);
        float p = fmodf(t * 2.0f + i * 0.18f, 1.0f);
        int16_t bh = (int16_t)lerp(8.0f, 18.0f, p);
        int16_t by = (int16_t)(CY + 14 + p * 70);
        uint8_t op = (uint8_t)((1.0f - p * 0.6f) * 255.0f);
        gfx.fillRoundedRect(x - 2, by, 5, bh, 2, colors.accent, op);
    }
}

// crying-sob: whole-face sob bob + a big teardrop falling from each side
static void render_crying_sob(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sob = sinf(t * TAU * 3.0f) * 3.0f;
    gfx.pushTransform();
    gfx.translate(0.0f, sob);
    int16_t hw = EYE_W / 2;
    int16_t cy = CY + 8;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 22, LX + hw, cy, colors.eye, 12);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 22, RX + hw, cy, colors.eye, 12);
    for (int side = 0; side < 2; side++) {
        int16_t x = (side == 0) ? (int16_t)(LX - 30) : (int16_t)(RX + 30);
        float p = fmodf(t * 1.4f + side * 0.5f, 1.0f);
        int16_t cy0 = (int16_t)(CY + 16 + p * 70);
        uint8_t op = (uint8_t)((1.0f - p * 0.4f) * 255.0f);
        gfx.fillEllipse(x, (int16_t)(cy0 + 16), 7, 16, colors.accent, op);
    }
    gfx.popTransform();
}

// crying-trickle: single tear forms and falls slowly from each eye
static void render_crying_trickle(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = EYE_W / 2;
    int16_t cy = CY + 8;
    int16_t h = (int16_t)(EYE_H * 0.72f);
    gfx.drawEye(LX, cy, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, cy, EYE_W, h, EYE_RX, 0, colors.eye);

    // Single tear from each eye, slow fall
    for (int side = 0; side < 2; side++) {
        int16_t ex = (side == 0) ? LX : RX;
        float p = fmodf(t + side * 0.5f, 1.0f);
        if (p < 0.2f) {
            // Forming: growing circle at eye corner
            float grow = p / 0.2f;
            int16_t r = (int16_t)(2.0f + grow * 3.0f);
            gfx.fillCircle((int16_t)(ex + (side == 0 ? -20 : 20)), cy + h / 2, r, colors.accent, 200);
        } else {
            // Falling
            float fall = (p - 0.2f) / 0.8f;
            int16_t tx = (int16_t)(ex + (side == 0 ? -20 : 20));
            int16_t ty = (int16_t)(cy + h / 2 + fall * 70);
            uint8_t op = (uint8_t)((1.0f - fall) * 220.0f);
            gfx.fillEllipse(tx, ty, 4, (int16_t)(6 + fall * 4), colors.accent, op);
        }
    }
}

const VariantDef CRYING_VARIANTS[] = {
    {"crying-tears",     "Tears",     2400, TONE_NONE, render_crying_tears},
    {"crying-waterfall", "Waterfall", 1600, TONE_NONE, render_crying_waterfall},
    {"crying-sob",       "Sob",       1200, TONE_NONE, render_crying_sob},
    {"crying-trickle",   "Trickle",   2800, TONE_NONE, render_crying_trickle},
};

extern const CategoryDef CAT_CRYING = {
    "crying", "Crying", ContentKind::EMOTION, TONE_BLUE,
    CRYING_VARIANTS, sizeof(CRYING_VARIANTS) / sizeof(CRYING_VARIANTS[0])
};
