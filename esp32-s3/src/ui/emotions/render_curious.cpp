#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// curious-lean: eyes tilt together (rotate around center), one slightly larger, head-tilt feel
static void render_curious_lean(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU * 0.5f) * 12.0f;
    float scaleR = 1.0f + sinf(t * TAU * 0.5f) * 0.08f;

    gfx.pushTransform();
    gfx.rotate(tilt, SCX, CY);

    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    // Right eye slightly larger
    int16_t rw = (int16_t)(EYE_W * scaleR);
    int16_t rh = (int16_t)(EYE_H * scaleR);
    gfx.drawEye(RX, CY, rw, rh, EYE_RX, 0, colors.eye);

    gfx.popTransform();
}

// curious-sparkle: wide eyes with small sparkle dots in each eye, slight bob
static void render_curious_sparkle(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 1.5f) * 4.0f;
    int16_t h = (int16_t)(EYE_H * 1.05f);
    int16_t w = (int16_t)(EYE_W * 1.02f);
    int16_t y = (int16_t)(CY + bob);

    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);

    // Sparkle dots inside each eye (small bright circles)
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.5f + i * 0.33f, 1.0f);
        float op = (p < 0.5f) ? p * 2.0f : (1.0f - p) * 2.0f;
        op = clamp(op, 0.2f, 1.0f);
        float angle = t * TAU * 2.0f + i * (TAU / 3.0f);
        float sr = 12.0f + i * 4.0f;

        int16_t lsx = (int16_t)(LX + cosf(angle) * sr);
        int16_t lsy = (int16_t)(y + sinf(angle) * sr * 0.6f);
        gfx.fillCircle(lsx, lsy, (int16_t)(2 + i), colors.accent,
                       (uint8_t)(op * 220.0f));

        int16_t rsx = (int16_t)(RX + cosf(angle + PI) * sr);
        int16_t rsy = (int16_t)(y + sinf(angle + PI) * sr * 0.6f);
        gfx.fillCircle(rsx, rsy, (int16_t)(2 + i), colors.accent,
                       (uint8_t)(op * 220.0f));
    }
}

// curious-scan: eyes scan left-right with pupil dots tracking, slight width changes
static void render_curious_scan(GfxEngine& gfx, float t, const ColorContext& colors) {
    float scan = sinf(t * TAU * 0.8f);
    float dx = scan * 14.0f;

    // Width subtly changes with scan direction
    float wScale = 1.0f + fabsf(scan) * 0.06f;
    int16_t w = (int16_t)(EYE_W * wScale);
    int16_t h = (int16_t)(EYE_H * 0.85f);

    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);

    // Pupil dots that track the scan direction
    int16_t pr = 10;
    gfx.fillCircle((int16_t)(LX + dx), CY, pr, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), CY, pr, colors.bg);
}

// --- Category registration ---
const VariantDef CURIOUS_VARIANTS[] = {
    {"curious-lean",    "Lean",    2800, TONE_NONE, render_curious_lean},
    {"curious-sparkle", "Sparkle", 3000, TONE_NONE, render_curious_sparkle},
    {"curious-scan",    "Scan",    2600, TONE_NONE, render_curious_scan},
};

extern const CategoryDef CAT_CURIOUS = {
    "curious", "Curious", ContentKind::EMOTION, TONE_ORANGE,
    CURIOUS_VARIANTS, sizeof(CURIOUS_VARIANTS) / sizeof(CURIOUS_VARIANTS[0])
};
