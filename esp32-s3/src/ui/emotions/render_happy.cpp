#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// happy-arc: smile arcs (quad bezier) with vertical bob
static void render_happy_arc(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 2.0f) * 3.0f;
    float d = 22.0f + sinf(t * TAU) * 2.0f;
    int16_t cy = (int16_t)(CY + 10 + bob);
    int16_t hw = (EYE_W - 10) / 2;

    // Left smile arc: quadratic bezier from (LX-hw, cy) ctrl (LX, cy+d) to (LX+hw, cy)
    gfx.drawQuadBezier(LX - hw, cy, LX, (int16_t)(cy + d), LX + hw, cy,
                       colors.eye, 14);
    // Right smile arc
    gfx.drawQuadBezier(RX - hw, cy, RX, (int16_t)(cy + d), RX + hw, cy,
                       colors.eye, 14);
}

// happy-bouncy: eyes bounce up and down with squash/stretch
static void render_happy_bouncy(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bounce = fabsf(sinf(t * TAU * 2.0f)) * 10.0f;
    float sq = 1.0f + sinf(t * TAU * 2.0f) * 0.06f;
    int16_t w = (int16_t)((float)EYE_W / sq);
    int16_t h = (int16_t)((float)EYE_H * sq);
    int16_t y = (int16_t)(CY - bounce);
    gfx.drawEye(LX, y, w, h, 32, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, 32, 0, colors.eye);
}

// happy-closed: closed smile arcs + tiny mouth
static void render_happy_closed(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU) * 1.5f;
    int16_t cy = (int16_t)(CY + 4 + wob);
    int16_t hw = (EYE_W - 4) / 2;
    int16_t depth = 26;

    // Left closed eye
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + depth, LX + hw, cy,
                       colors.eye, 12);
    // Right closed eye
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + depth, RX + hw, cy,
                       colors.eye, 12);
    // Tiny mouth arc at bottom center (0.7 opacity)
    int16_t mx = SCREEN_W / 2;
    int16_t my = SCREEN_H - 36;
    gfx.pushAlpha(178);
    gfx.drawQuadBezier(mx - 20, my, mx, my + 8, mx + 20, my,
                       colors.eye, 5);
    gfx.popAlpha();
}

// happy-cheek: squished eyes + cheek dots
static void render_happy_cheek(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sq = 1.0f + sinf(t * TAU * 2.0f) * 0.04f;
    int16_t h = (int16_t)((float)EYE_H * 0.55f * sq);
    gfx.drawEye(LX, CY - 6, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY - 6, EYE_W, h, EYE_RX, 0, colors.eye);
    // Cheek dots
    gfx.fillCircle(LX, CY + 36, 6, colors.accent, 204); // 0.8 opacity
    gfx.fillCircle(RX, CY + 36, 6, colors.accent, 204);
}

// happy-laugh: rapid vertical shake + closed smile arcs
static void render_happy_laugh(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 8.0f) * 4.0f;
    int16_t cy = (int16_t)(CY + 6 + shake);
    int16_t hw = (EYE_W - 8) / 2;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 24, LX + hw, cy, colors.eye, 13);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 24, RX + hw, cy, colors.eye, 13);
}

// happy-giggle: asymmetric smile wobble, eyes at different heights
static void render_happy_giggle(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 3.0f) * 3.0f;
    int16_t hw = (EYE_W - 10) / 2;
    int16_t lyc = (int16_t)(CY + 8 + wob);
    int16_t ryc = (int16_t)(CY + 8 - wob * 0.6f);
    gfx.drawQuadBezier(LX - hw, lyc, LX, lyc + 20, LX + hw, lyc, colors.eye, 12);
    gfx.drawQuadBezier(RX - hw, ryc, RX, ryc + 22, RX + hw, ryc, colors.eye, 12);
}

// happy-shimmer: closed smile + floating sparkle dots rising
static void render_happy_shimmer(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = (EYE_W - 8) / 2;
    int16_t cy = CY + 6;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 22, LX + hw, cy, colors.eye, 12);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 22, RX + hw, cy, colors.eye, 12);

    for (int i = 0; i < 5; i++) {
        float p = fmodf(t * 1.5f + i * 0.2f, 1.0f);
        int16_t x = (int16_t)(60 + i * 50 + sinf(p * TAU + (float)i) * 6.0f);
        int16_t y = (int16_t)lerp((float)(SCREEN_H - 20), (float)(STATUS_H + 10), p);
        int16_t r = (int16_t)lerp(3.0f, 1.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 220.0f);
        gfx.fillCircle(x, y, r, colors.accent, op);
    }
}

// happy-skip: bouncy eyes with horizontal bob
static void render_happy_skip(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bounce = fabsf(sinf(t * TAU * 2.0f)) * 8.0f;
    float hob = sinf(t * TAU) * 6.0f;
    int16_t y = (int16_t)(CY - bounce);
    gfx.drawEye((int16_t)(LX + hob), y, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + hob), y, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
}

// --- Category registration ---
const VariantDef HAPPY_VARIANTS[] = {
    {"happy-arc",     "Smile arc",     1800, TONE_NONE, render_happy_arc},
    {"happy-bouncy",  "Bouncy",        1200, TONE_NONE, render_happy_bouncy},
    {"happy-closed",  "Closed smile",  2400, TONE_NONE, render_happy_closed},
    {"happy-cheek",   "Cheek up",      2000, TONE_NONE, render_happy_cheek},
    {"happy-laugh",   "Laugh",          900, TONE_NONE, render_happy_laugh},
    {"happy-giggle",  "Giggle",        1400, TONE_NONE, render_happy_giggle},
    {"happy-shimmer", "Shimmer",       2000, TONE_NONE, render_happy_shimmer},
    {"happy-skip",    "Skip",          1100, TONE_NONE, render_happy_skip},
};

extern const CategoryDef CAT_HAPPY = {
    "happy", "Happy", ContentKind::EMOTION, TONE_WARM,
    HAPPY_VARIANTS, sizeof(HAPPY_VARIANTS) / sizeof(HAPPY_VARIANTS[0])
};
