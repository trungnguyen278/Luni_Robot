#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// suspicious-squint: very narrow eyes, slow side-scan motion
static void render_suspicious_squint(GfxEngine& gfx, float t, const ColorContext& colors) {
    float scan = sinf(t * TAU * 0.5f) * 6.0f;
    int16_t h = (int16_t)(EYE_H * 0.35f);

    gfx.drawEye((int16_t)(LX + scan), CY + 4, EYE_W, h, 8, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + scan), CY + 4, EYE_W, h, 8, 0, colors.eye);

    // Small bg pupils tracking with scan
    float pupilShift = sinf(t * TAU * 0.5f) * 12.0f;
    gfx.fillCircle((int16_t)(LX + scan + pupilShift), CY + 4, 5, colors.bg);
    gfx.fillCircle((int16_t)(RX + scan + pupilShift), CY + 4, 5, colors.bg);
}

// suspicious-side: narrow eyes with pupils far to one side, one eye slightly more open
static void render_suspicious_side(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 0.6f) * 1.0f;
    int16_t lh = (int16_t)(EYE_H * 0.4f);
    int16_t rh = (int16_t)(EYE_H * 0.5f);

    gfx.drawEye(LX, (int16_t)(CY + 4 + bob), EYE_W, lh, 10, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + 4 - bob), EYE_W, rh, 12, 0, colors.eye);

    // Pupils far to one side (left)
    float look = -16.0f + sinf(t * TAU * 0.4f) * 2.0f;
    gfx.fillCircle((int16_t)(LX + look), (int16_t)(CY + 4 + bob), 6, colors.bg);
    gfx.fillCircle((int16_t)(RX + look), (int16_t)(CY + 4 - bob), 7, colors.bg);
}

// suspicious-scan: narrow eyes with pupils scanning left-right in steps
static void render_suspicious_scan(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t h = (int16_t)(EYE_H * 0.4f);
    gfx.drawEye(LX, CY + 4, EYE_W, h, 10, 0, colors.eye);
    gfx.drawEye(RX, CY + 4, EYE_W, h, 10, 0, colors.eye);

    // Step-based scanning: quantize sinf into 3 positions (left, center, right)
    float raw = sinf(t * TAU * 0.7f);
    float stepped;
    if (raw < -0.33f) stepped = -1.0f;
    else if (raw > 0.33f) stepped = 1.0f;
    else stepped = 0.0f;
    float pupilX = stepped * 16.0f;

    gfx.fillCircle((int16_t)(LX + pupilX), CY + 4, 6, colors.bg);
    gfx.fillCircle((int16_t)(RX + pupilX), CY + 4, 6, colors.bg);
}

// --- Category registration ---
const VariantDef SUSPICIOUS_VARIANTS[] = {
    {"suspicious-squint", "Squint", 2800, TONE_NONE, render_suspicious_squint},
    {"suspicious-side",   "Side",   2600, TONE_NONE, render_suspicious_side},
    {"suspicious-scan",   "Scan",   3000, TONE_NONE, render_suspicious_scan},
};

extern const CategoryDef CAT_SUSPICIOUS = {
    "suspicious", "Suspicious", ContentKind::EMOTION, TONE_PURPLE,
    SUSPICIOUS_VARIANTS, sizeof(SUSPICIOUS_VARIANTS) / sizeof(SUSPICIOUS_VARIANTS[0])
};
