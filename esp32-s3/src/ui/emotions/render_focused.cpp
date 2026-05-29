#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// focused-laser: very narrow eyes with bright center dots, no distraction
static void render_focused_laser(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sh = sinf(t * TAU * 4.0f) * 0.5f;
    int16_t h = (int16_t)(EYE_H * 0.4f);
    gfx.drawEye(LX, CY, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 10, 0, colors.eye);

    // Bright center dots (pulsing intensity)
    float pulse_val = (sinf(t * TAU * 3.0f) + 1.0f) / 2.0f;
    uint8_t dotAlpha = (uint8_t)(180 + pulse_val * 75.0f);
    int16_t dotR = (int16_t)(4.0f + pulse_val * 2.0f);
    gfx.fillCircle(LX, (int16_t)(CY + sh), dotR, colors.accent, dotAlpha);
    gfx.fillCircle(RX, (int16_t)(CY - sh), dotR, colors.accent, dotAlpha);
}

// focused-scan: eyes have bg pupil circles that scan slowly left-right
static void render_focused_scan(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, CY, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 14, 0, colors.eye);

    // Scanning pupil circles
    float scan = sinf(t * TAU * 0.4f) * 12.0f;
    gfx.fillCircle((int16_t)(LX + scan), CY, 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + scan), CY, 11, colors.bg);

    // Tiny bright center in each pupil
    gfx.fillCircle((int16_t)(LX + scan), CY, 3, colors.accent, 200);
    gfx.fillCircle((int16_t)(RX + scan), CY, 3, colors.accent, 200);
}

// focused-target: eyes with concentric target rings, crosshair lines
static void render_focused_target(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.55f);
    gfx.drawEye(LX, CY, EYE_W, h, 14, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, 14, 0, colors.eye);

    // Concentric rings inside each eye
    float pulse_val = sinf(t * TAU * 2.0f) * 0.1f;
    for (int i = 0; i < 3; i++) {
        int16_t r = (int16_t)((18 - i * 5) * (1.0f + pulse_val));
        gfx.strokeCircle(LX, CY, r, colors.bg, 2);
        gfx.strokeCircle(RX, CY, r, colors.bg, 2);
    }

    // Crosshair lines
    int16_t chLen = 14;
    gfx.drawLine(LX - chLen, CY, LX + chLen, CY, colors.bg, 1, 160);
    gfx.drawLine(LX, CY - chLen, LX, CY + chLen, colors.bg, 1, 160);
    gfx.drawLine(RX - chLen, CY, RX + chLen, CY, colors.bg, 1, 160);
    gfx.drawLine(RX, CY - chLen, RX, CY + chLen, colors.bg, 1, 160);

    // Center dot
    gfx.fillCircle(LX, CY, 3, colors.accent, 230);
    gfx.fillCircle(RX, CY, 3, colors.accent, 230);
}

// focused-pinpoint: eyes narrow then widen in a breathing pattern, tiny bright center dot
static void render_focused_pinpoint(GfxEngine& gfx, float t, const ColorContext& colors) {
    float breath = (sinf(t * TAU * 0.8f) + 1.0f) / 2.0f; // 0..1
    float hScale = lerp(0.4f, 0.7f, breath);
    int16_t h = (int16_t)(EYE_H * hScale);
    int16_t rx = (int16_t)lerp(8.0f, 18.0f, breath);

    gfx.drawEye(LX, CY, EYE_W, h, rx, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, rx, 0, colors.eye);

    // Tiny bright center dot (constant)
    uint8_t dotAlpha = (uint8_t)(200 + breath * 55.0f);
    gfx.fillCircle(LX, CY, 3, colors.accent, dotAlpha);
    gfx.fillCircle(RX, CY, 3, colors.accent, dotAlpha);
}

// --- Category registration ---
const VariantDef FOCUSED_VARIANTS[] = {
    {"focused-laser",    "Laser",    2800, TONE_NONE, render_focused_laser},
    {"focused-scan",     "Scan",     3000, TONE_NONE, render_focused_scan},
    {"focused-target",   "Target",   2600, TONE_NONE, render_focused_target},
    {"focused-pinpoint", "Pinpoint", 2400, TONE_NONE, render_focused_pinpoint},
};

extern const CategoryDef CAT_FOCUSED = {
    "focused", "Focused", ContentKind::EMOTION, TONE_RED,
    FOCUSED_VARIANTS, sizeof(FOCUSED_VARIANTS) / sizeof(FOCUSED_VARIANTS[0])
};
