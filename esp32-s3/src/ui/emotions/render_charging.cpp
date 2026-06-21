#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

// Helper: draw a lightning bolt shape
static void drawBolt(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                     uint16_t color, uint8_t alpha = 255) {
    float k = s / 20.0f;
    // Lightning bolt as two connected triangles
    int16_t x0 = (int16_t)(cx - 4 * k);
    int16_t y0 = (int16_t)(cy - 14 * k);
    int16_t x1 = (int16_t)(cx + 6 * k);
    int16_t y1 = (int16_t)(cy - 2 * k);
    int16_t x2 = (int16_t)(cx - 1 * k);
    int16_t y2 = (int16_t)(cy - 2 * k);

    int16_t x3 = (int16_t)(cx + 4 * k);
    int16_t y3 = (int16_t)(cy + 14 * k);
    int16_t x4 = (int16_t)(cx - 6 * k);
    int16_t y4 = (int16_t)(cy + 2 * k);
    int16_t x5 = (int16_t)(cx + 1 * k);
    int16_t y5 = (int16_t)(cy + 2 * k);

    gfx.fillTriangle(x0, y0, x1, y1, x2, y2, color, alpha);
    gfx.fillTriangle(x5, y5, x4, y4, x3, y3, color, alpha);
}

// charging-bolt: narrow eyes + a pulsing lightning bolt overhead
static void render_charging_bolt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pulseB = 0.5f + fabsf(sinf(t * TAU)) * 0.5f;
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    drawBolt(gfx, SCREEN_W / 2, STATUS_H + 28, 22.0f, colors.accent,
             (uint8_t)(pulseB * 255.0f));
}

// charging-fill: eyes with progress bar below them that fills left-to-right over time
static void render_charging_fill(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU) * 2.0f;
    int16_t h = (int16_t)(EYE_H * 0.8f);
    gfx.drawEye(LX, (int16_t)(CY - 6 + bob), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY - 6 + bob), EYE_W, h, EYE_RX, 0, colors.eye);

    // Progress bar below eyes
    int16_t bx = 50, by = SCREEN_H - 32;
    int16_t bw = SCREEN_W - 100, bh = 10;
    gfx.strokeRoundedRect(bx, by, bw, bh, 4, colors.accent, 1);

    // Fill: cycles through 0..1 over the animation period
    float fill = fmodf(t, 1.0f);
    int16_t fw = (int16_t)((float)(bw - 4) * fill);
    if (fw > 0) {
        gfx.fillRoundedRect(bx + 2, by + 2, fw, bh - 4, 2, colors.accent);
    }

    // Percentage text
    int pct = (int)(fill * 100.0f);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    gfx.drawText(buf, SCX, by - 12, colors.accent, 1, GfxEngine::TextAlign::CENTER);
}

// charging-spark: eyes + small spark particles radiating outward from center
static void render_charging_spark(GfxEngine& gfx, float t, const ColorContext& colors) {
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    // Spark particles radiating from center
    for (int i = 0; i < 6; i++) {
        float p = fmodf(t * 1.6f + i * (1.0f / 6.0f), 1.0f);
        float angle = i * (TAU / 6.0f) + t * TAU * 0.3f;
        float dist = p * 60.0f;

        int16_t sx = (int16_t)(SCX + cosf(angle) * dist);
        int16_t sy = (int16_t)(CY + sinf(angle) * dist * 0.7f);
        float r = lerp(4.0f, 1.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 230.0f);

        gfx.fillCircle(sx, sy, (int16_t)r, colors.accent, op);

        // Tiny tail line
        if (p > 0.1f && p < 0.8f) {
            float prevDist = dist - 8.0f;
            int16_t tx = (int16_t)(SCX + cosf(angle) * prevDist);
            int16_t ty = (int16_t)(CY + sinf(angle) * prevDist * 0.7f);
            gfx.drawLine(tx, ty, sx, sy, colors.accent, 1, (uint8_t)(op * 0.5f));
        }
    }
}

// charging-glow: eyes that subtly pulse brighter/dimmer (alpha cycling)
static void render_charging_glow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float glow = (sinf(t * TAU * 0.8f) + 1.0f) / 2.0f;
    uint8_t eyeAlpha = (uint8_t)(140 + glow * 115.0f);

    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye, eyeAlpha);
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye, eyeAlpha);

    // Glow aura around eyes
    uint8_t auraAlpha = (uint8_t)(glow * 60.0f);
    int16_t aw = (int16_t)(EYE_W + 16);
    int16_t ah = (int16_t)(EYE_H + 16);
    gfx.drawEye(LX, CY, aw, ah, EYE_RX + 6, 0, colors.accent, auraAlpha);
    gfx.drawEye(RX, CY, aw, ah, EYE_RX + 6, 0, colors.accent, auraAlpha);
}

// --- Category registration ---
const VariantDef CHARGING_VARIANTS[] = {
    {"charging-bolt",  "Bolt",  2800, TONE_NONE, render_charging_bolt},
    {"charging-fill",  "Fill",  3000, TONE_NONE, render_charging_fill},
    {"charging-spark", "Spark", 2600, TONE_NONE, render_charging_spark},
    {"charging-glow",  "Glow",  3200, TONE_NONE, render_charging_glow},
};

extern const CategoryDef CAT_CHARGING = {
    "charging", "Charging", ContentKind::EMOTION, TONE_GREEN,
    CHARGING_VARIANTS, sizeof(CHARGING_VARIANTS) / sizeof(CHARGING_VARIANTS[0])
};
