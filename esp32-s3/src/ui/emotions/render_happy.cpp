#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Simple 5-pointed star using triangles (matches JSX starPath).
static void fillStar(GfxEngine& gfx, int16_t cx, int16_t cy, float r1, float r2,
                     uint16_t color, uint8_t alpha = 255) {
    for (int i = 0; i < 5; i++) {
        float a1 = -1.5708f + i * (TAU / 5.0f);
        float a2 = a1 + TAU / 10.0f;
        float a3 = a1 + TAU / 5.0f;
        gfx.fillTriangle((int16_t)(cx + cosf(a1) * r1), (int16_t)(cy + sinf(a1) * r1),
                         (int16_t)(cx + cosf(a2) * r2), (int16_t)(cy + sinf(a2) * r2),
                         cx, cy, color, alpha);
        gfx.fillTriangle((int16_t)(cx + cosf(a2) * r2), (int16_t)(cy + sinf(a2) * r2),
                         (int16_t)(cx + cosf(a3) * r1), (int16_t)(cy + sinf(a3) * r1),
                         cx, cy, color, alpha);
    }
}

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

// happy-laugh: shaking squashed smiles + big open mouth arc
static void render_happy_laugh(GfxEngine& gfx, float t, const ColorContext& colors) {
    float shake = sinf(t * TAU * 4.0f) * 3.0f;
    float sq = 1.0f + fabsf(sinf(t * TAU * 2.0f)) * 0.15f;
    int16_t hw = (int16_t)(((float)(EYE_W - 4) / sq) / 2.0f);
    int16_t cy = CY + 4;
    int16_t lx = (int16_t)(LX + shake), rx = (int16_t)(RX + shake);
    gfx.drawQuadBezier((int16_t)(lx - hw), cy, lx, cy + 26, (int16_t)(lx + hw), cy, colors.eye, 13);
    gfx.drawQuadBezier((int16_t)(rx - hw), cy, rx, cy + 26, (int16_t)(rx + hw), cy, colors.eye, 13);
    // open laughing mouth
    int16_t mx = SCREEN_W / 2, my = SCREEN_H - 32;
    gfx.drawQuadBezier(mx - 30, my, mx, my + 14, mx + 30, my, colors.eye, 6);
}

// happy-giggle: counter-bobbing smiles with gentle squash
static void render_happy_giggle(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wob = sinf(t * TAU * 5.0f) * 4.0f;
    float sq = 1.0f + sinf(t * TAU * 3.0f) * 0.07f;
    int16_t hw = (int16_t)(((float)(EYE_W - 4) * sq) / 2.0f);
    int16_t lyc = (int16_t)(CY + 4 + wob);
    int16_t ryc = (int16_t)(CY + 4 - wob);
    gfx.drawQuadBezier((int16_t)(LX - hw), lyc, LX, lyc + 22, (int16_t)(LX + hw), lyc, colors.eye, 13);
    gfx.drawQuadBezier((int16_t)(RX - hw), ryc, RX, ryc + 22, (int16_t)(RX + hw), ryc, colors.eye, 13);
}

// happy-shimmer: closed smiles + 3 rising sparkle stars
static void render_happy_shimmer(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = (EYE_W - 4) / 2;
    int16_t cy = CY + 6;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 20, LX + hw, cy, colors.eye, 13);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 20, RX + hw, cy, colors.eye, 13);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 1.5f + i * 0.33f, 1.0f);
        int16_t x = (int16_t)(SCREEN_W / 2 + (i - 1) * 60);
        int16_t y = (int16_t)(STATUS_H + 16 + p * 20.0f);
        float s = lerp(7.0f, 2.0f, p);
        fillStar(gfx, x, y, s, s * 0.4f, colors.accent, (uint8_t)((1.0f - p) * 255.0f));
    }
}

// happy-skip: hop with synced horizontal sway, slightly tall eyes
static void render_happy_skip(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bx = sinf(t * TAU * 2.0f) * 5.0f;
    float dy = -fabsf(sinf(t * TAU * 2.0f)) * 8.0f;
    int16_t y = (int16_t)(CY + dy);
    int16_t h = (int16_t)((float)EYE_H * 1.05f);
    gfx.drawEye((int16_t)(LX + bx), y, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + bx), y, EYE_W, h, EYE_RX, 0, colors.eye);
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
