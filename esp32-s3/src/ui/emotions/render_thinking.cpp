#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// thinking-look-up: eyes glance up-left with pulsing dots above
static void render_thinking_look_up(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU * 0.5f);
    float dx = -10.0f + phase * 6.0f;
    float dy = -16.0f;

    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), (int16_t)(CY + dy), 12, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), (int16_t)(CY + dy), 12, colors.bg);

    // Pulsing dots above head
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.0f + i * 0.2f, 1.0f);
        float op = p < 0.5f ? p * 2.0f : (1.0f - p) * 2.0f;
        op = clamp(op, 0.25f, 1.0f);
        gfx.fillCircle((int16_t)(LX + 10 + i * 14), STATUS_H + 12,
                       3, colors.accent, (uint8_t)(op * 255.0f));
    }
}

// thinking-spinner: eyes with rotating arc in corner
static void render_thinking_spinner(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.8f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);

    // Spinning arc in bottom-right corner
    float ang = t * TAU;
    int16_t sx = SCREEN_W - 30;
    int16_t sy = SCREEN_H - 30;
    gfx.drawArc(sx, sy, 10, ang, ang + 3.14159f,
                colors.accent, 4);
}

// thinking-scanning: pupils scan left-right + scan line at bottom
static void render_thinking_scanning(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU - 1.5708f); // -PI/2
    float dx = phase * 14.0f;

    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), CY, 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), CY, 11, colors.bg);

    // Scan line at bottom
    int16_t lineY = SCREEN_H - 14;
    gfx.drawLine(20, lineY, SCREEN_W - 20, lineY, colors.accent, 1, 64);
    float scanX = lerp(20.0f, (float)(SCREEN_W - 20),
                       (sinf(t * TAU - 1.5708f) + 1.0f) / 2.0f);
    gfx.fillCircle((int16_t)scanX, lineY, 3, colors.accent);
}

// thinking-dots: smaller eyes + animated dots above
static void render_thinking_dots(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.7f);
    gfx.drawEye(LX, CY + 10, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 10, EYE_W, h, EYE_RX, 0, colors.eye);

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t - i * 0.15f + 1.0f, 1.0f);
        float r = p < 0.4f ? 3.0f + (p / 0.4f) * 5.0f
                           : 8.0f - (p - 0.4f) * 8.0f;
        float op = p < 0.6f ? 1.0f : 1.0f - (p - 0.6f) * 2.0f;
        r = clamp(r, 2.5f, 8.0f);
        op = clamp(op, 0.3f, 1.0f);
        gfx.fillCircle((int16_t)(SCREEN_W / 2 - 22 + i * 22), CY - 50,
                       (int16_t)r, colors.accent, (uint8_t)(op * 255.0f));
    }
}

// thinking-hmm: narrow asymmetric eyes (one higher) + small dot at mouth
static void render_thinking_hmm(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU * 0.5f);
    int16_t lh = (int16_t)(EYE_H * 0.55f);
    int16_t rh = (int16_t)(EYE_H * 0.7f);
    int16_t ldy = (int16_t)(phase * 4.0f);
    gfx.drawEye(LX, CY + ldy, EYE_W, lh, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY - 6, EYE_W, rh, EYE_RX, 0, colors.eye);

    // Small dot at mouth position
    float dp = (sinf(t * TAU) + 1.0f) / 2.0f;
    int16_t dr = (int16_t)lerp(2.0f, 4.0f, dp);
    gfx.fillCircle(SCX + 4, SCREEN_H - 32, dr, colors.eye, 180);
}

// thinking-aha: eyes start narrow, then pop wide + spark burst above
static void render_thinking_aha(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pop;
    if (t < 0.6f) {
        pop = 0.0f;
    } else if (t < 0.75f) {
        pop = ease::out((t - 0.6f) / 0.15f);
    } else {
        pop = 1.0f;
    }

    int16_t h = (int16_t)lerp(EYE_H * 0.5f, EYE_H * 1.15f, pop);
    int16_t w = (int16_t)lerp((float)EYE_W, EYE_W * 1.1f, pop);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Spark burst above when popping
    if (pop > 0.1f) {
        uint8_t op = (uint8_t)(pop * 220.0f);
        for (int i = 0; i < 6; i++) {
            float a = (float)i * (TAU / 6.0f) - PI / 2.0f;
            float r = 12.0f + pop * 16.0f;
            int16_t sx = (int16_t)(SCX + cosf(a) * r);
            int16_t sy = (int16_t)(STATUS_H + 22 + sinf(a) * r * 0.6f);
            gfx.fillCircle(sx, sy, 3, colors.accent, op);
        }
    }
}

// thinking-cog: closed eyes + rotating gear/cog circles above
static void render_thinking_cog(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t hw = (EYE_W - 10) / 2;
    int16_t cy = CY + 8;
    gfx.drawQuadBezier(LX - hw, cy, LX, cy + 14, LX + hw, cy, colors.eye, 10);
    gfx.drawQuadBezier(RX - hw, cy, RX, cy + 14, RX + hw, cy, colors.eye, 10);

    // Rotating cog: center circle + teeth
    float rot = t * TAU * 2.0f;
    int16_t gx_c = SCX;
    int16_t gy = STATUS_H + 26;
    gfx.strokeCircle(gx_c, gy, 10, colors.accent, 3);
    for (int i = 0; i < 8; i++) {
        float a = rot + (float)i * (TAU / 8.0f);
        int16_t tx = (int16_t)(gx_c + cosf(a) * 14.0f);
        int16_t ty = (int16_t)(gy + sinf(a) * 14.0f);
        gfx.fillCircle(tx, ty, 3, colors.accent);
    }
}

// --- Category registration ---
const VariantDef THINKING_VARIANTS[] = {
    {"thinking-look-up",  "Look up",    2800, TONE_NONE, render_thinking_look_up},
    {"thinking-spinner",  "Spinner",    1200, TONE_NONE, render_thinking_spinner},
    {"thinking-scanning", "Scanning",   2400, TONE_NONE, render_thinking_scanning},
    {"thinking-dots",     "Dots above", 1500, TONE_NONE, render_thinking_dots},
    {"thinking-hmm",      "Hmm",        2400, TONE_NONE, render_thinking_hmm},
    {"thinking-aha",      "Aha!",       1800, TONE_NONE, render_thinking_aha},
    {"thinking-cog",      "Cog",        2400, TONE_NONE, render_thinking_cog},
};

extern const CategoryDef CAT_THINKING = {
    "thinking", "Thinking", ContentKind::EMOTION, TONE_CYAN,
    THINKING_VARIANTS, sizeof(THINKING_VARIANTS) / sizeof(THINKING_VARIANTS[0])
};
