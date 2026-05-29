#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// hungry-eye-food: wide sparkling eyes, drool indicator, pupils dilated
static void render_hungry_eye_food(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 1.5f) * 3.0f;
    int16_t w = (int16_t)(EYE_W * 1.08f);
    int16_t h = (int16_t)(EYE_H * 1.08f);
    int16_t y = (int16_t)(CY + bob);

    gfx.drawEye(LX, y, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, y, w, h, EYE_RX, 0, colors.eye);

    // Dilated pupils (dark bg circles)
    gfx.fillCircle(LX, y, 16, colors.bg);
    gfx.fillCircle(RX, y, 16, colors.bg);

    // Sparkle dots in eyes (small bright points)
    for (int i = 0; i < 2; i++) {
        float p = fmodf(t * 3.0f + i * 0.5f, 1.0f);
        float op = (p < 0.5f) ? p * 2.0f : (1.0f - p) * 2.0f;
        int16_t sx = (i == 0) ? (int16_t)(LX - 6) : (int16_t)(RX - 6);
        int16_t sy = (int16_t)(y - 8);
        gfx.fillCircle(sx, sy, 3, colors.accent, (uint8_t)(op * 255.0f));
        gfx.fillCircle((int16_t)(sx + 10), (int16_t)(sy + 4), 2, colors.accent,
                       (uint8_t)(op * 180.0f));
    }

    // Drool drop on right side
    float droolP = fmodf(t * 0.8f, 1.0f);
    int16_t dx = RX + EYE_W / 2 + 4;
    int16_t dy = (int16_t)(CY + 10 + droolP * 30.0f);
    uint8_t dop = (uint8_t)((1.0f - droolP * 0.5f) * 200.0f);
    gfx.fillCircle(dx, dy, (int16_t)(3 + droolP * 2), colors.accent, dop);
}

// hungry-drool: normal eyes + drool drops falling from one side, accumulating below
static void render_hungry_drool(GfxEngine& gfx, float t, const ColorContext& colors) {
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    // Drool drops falling from right side
    int16_t startX = RX + EYE_W / 2 + 2;
    int16_t startY = CY + EYE_H / 2;

    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.7f + i * 0.33f, 1.0f);
        int16_t dy = (int16_t)(startY + p * 60.0f);
        float wobble = sinf(p * PI * 2.0f + i) * 3.0f;
        int16_t dx = (int16_t)(startX + wobble);
        float r = lerp(4.0f, 2.5f, p);
        uint8_t op = (uint8_t)((1.0f - p * 0.5f) * 210.0f);

        // Drop shape: circle + small triangle above
        gfx.fillCircle(dx, dy, (int16_t)r, colors.accent, op);
        gfx.fillTriangle((int16_t)(dx - r * 0.6f), dy,
                         (int16_t)(dx + r * 0.6f), dy,
                         dx, (int16_t)(dy - r * 1.5f),
                         colors.accent, op);
    }

    // Accumulated puddle at bottom
    float puddleW = 20.0f + sinf(t * TAU * 0.5f) * 4.0f;
    int16_t py = SCREEN_H - 18;
    gfx.fillEllipse((int16_t)(startX - 2), py, (int16_t)puddleW, 5,
                    colors.accent, 140);
}

// hungry-lick: eyes + tongue arc sweeping across bottom (animated drawArc)
static void render_hungry_lick(GfxEngine& gfx, float t, const ColorContext& colors) {
    float bob = sinf(t * TAU * 1.2f) * 2.0f;
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    // Tongue arc sweeping across bottom
    float sweep = fmodf(t * 0.5f, 1.0f);
    float tongueAngle;
    if (sweep < 0.4f) {
        tongueAngle = ease::inOut(sweep / 0.4f) * PI;
    } else if (sweep < 0.6f) {
        tongueAngle = PI;
    } else {
        tongueAngle = PI + ease::inOut((sweep - 0.6f) / 0.4f) * PI;
    }

    int16_t ty = SCREEN_H - 28;
    float arcStart = tongueAngle - 0.6f;
    float arcEnd = tongueAngle + 0.6f;
    gfx.drawArc(SCX, ty, 30, arcStart, arcEnd, colors.accent, 6, 220);

    // Tongue tip: small filled circle at leading edge
    int16_t tipX = (int16_t)(SCX + cosf(arcEnd) * 30.0f);
    int16_t tipY = (int16_t)(ty + sinf(arcEnd) * 30.0f);
    gfx.fillCircle(tipX, tipY, 5, colors.accent, 220);
}

// hungry-stomach: eyes bob slightly + stomach growl lines (wavy lines at bottom center)
static void render_hungry_stomach(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Eyes bob with stomach growl rhythm
    float growl = sinf(t * TAU * 1.5f);
    float bob = growl * 3.0f;
    gfx.drawEye(LX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + bob), EYE_W, EYE_H, EYE_RX, 0, colors.eye);

    // Stomach growl: wavy lines at bottom center
    int16_t baseY = SCREEN_H - 24;
    float intensity = (growl + 1.0f) / 2.0f;
    uint8_t lineAlpha = (uint8_t)(80 + intensity * 140.0f);

    for (int line = 0; line < 3; line++) {
        int16_t ly = (int16_t)(baseY + line * 6);
        float amp = (3.0f - line) * 2.5f * intensity;
        int16_t prevX = SCX - 40;
        int16_t prevY = ly;

        for (int seg = 1; seg <= 8; seg++) {
            int16_t sx = (int16_t)(SCX - 40 + seg * 10);
            int16_t sy = (int16_t)(ly + sinf(t * TAU * 3.0f + seg * 0.8f + line) * amp);
            gfx.drawLine(prevX, prevY, sx, sy, colors.accent, 2, lineAlpha);
            prevX = sx;
            prevY = sy;
        }
    }
}

// --- Category registration ---
const VariantDef HUNGRY_VARIANTS[] = {
    {"hungry-eye-food", "Eye food", 2800, TONE_NONE, render_hungry_eye_food},
    {"hungry-drool",    "Drool",    3000, TONE_NONE, render_hungry_drool},
    {"hungry-lick",     "Lick",     2600, TONE_NONE, render_hungry_lick},
    {"hungry-stomach",  "Stomach",  2800, TONE_NONE, render_hungry_stomach},
};

extern const CategoryDef CAT_HUNGRY = {
    "hungry", "Hungry", ContentKind::EMOTION, TONE_ORANGE,
    HUNGRY_VARIANTS, sizeof(HUNGRY_VARIANTS) / sizeof(HUNGRY_VARIANTS[0])
};
