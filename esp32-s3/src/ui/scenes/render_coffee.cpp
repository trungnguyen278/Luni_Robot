#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry COFFEE_PHASES[] = {
    {"sleepy",  0.06f},   // Luni sleepy, centered
    {"cupIn",   0.07f},   // Cup slides in from bottom
    {"potIn",   0.06f},   // Coffee pot slides in from right
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"pour",    0.16f},   // Pot tilts, stream line, cup fills
    {"potOut",  0.06f},   // Pot exits right
    {"steam",   0.14f},   // Steam rises from cup
    {"sip",     0.10f},   // Cup tilts for sip
    {"cupOut",  0.06f},   // Cup exits bottom
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.17f},   // Satisfied/happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(COFFEE_PHASES) / sizeof(COFFEE_PHASES[0]);

/* -- Helper: draw coffee cup --------------------------------------------- */
static void drawCup(GfxEngine& gfx, int16_t cx, int16_t cy,
                    float fillLevel, uint16_t color, uint16_t bg,
                    uint8_t alpha) {
    gfx.pushAlpha(alpha);
    // Cup body outline
    gfx.drawQuadBezier((int16_t)(cx - 30), (int16_t)(cy - 4),
                       (int16_t)(cx - 30), (int16_t)(cy + 28),
                       cx, (int16_t)(cy + 28), color, 4);
    gfx.drawQuadBezier(cx, (int16_t)(cy + 28),
                       (int16_t)(cx + 30), (int16_t)(cy + 28),
                       (int16_t)(cx + 30), (int16_t)(cy - 4), color, 4);
    // Rim
    gfx.drawLine((int16_t)(cx - 30), (int16_t)(cy - 4),
                 (int16_t)(cx + 30), (int16_t)(cy - 4), color, 4);
    // Handle
    gfx.drawQuadBezier((int16_t)(cx + 30), (int16_t)(cy + 4),
                       (int16_t)(cx + 44), (int16_t)(cy + 8),
                       (int16_t)(cx + 38), (int16_t)(cy + 20), color, 4);

    // Coffee fill (from bottom up)
    if (fillLevel > 0.02f) {
        int16_t fillH = (int16_t)(26.0f * fillLevel);
        gfx.fillRect((int16_t)(cx - 26), (int16_t)(cy + 24 - fillH),
                     52, fillH, color);
    }
    gfx.popAlpha();
}

/* -- Helper: draw coffee pot --------------------------------------------- */
static void drawCoffeePot(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float tiltAngle, uint16_t color, uint8_t alpha) {
    gfx.pushTransform();
    gfx.rotate(tiltAngle, (float)cx, (float)cy);

    // Pot body
    gfx.fillRoundedRect((int16_t)(cx - 18), (int16_t)(cy - 24),
                        36, 48, 4, color, alpha);
    // Lid
    gfx.fillRect((int16_t)(cx - 14), (int16_t)(cy - 28), 28, 6, color, alpha);
    // Lid knob
    gfx.fillCircle(cx, (int16_t)(cy - 30), 4, color, alpha);
    // Handle (left side)
    gfx.drawLine((int16_t)(cx - 18), (int16_t)(cy - 14),
                 (int16_t)(cx - 30), (int16_t)(cy - 8), color, 5, alpha);
    gfx.drawLine((int16_t)(cx - 30), (int16_t)(cy - 8),
                 (int16_t)(cx - 30), (int16_t)(cy + 10), color, 5, alpha);
    // Spout (right side)
    gfx.drawLine((int16_t)(cx + 18), (int16_t)(cy - 16),
                 (int16_t)(cx + 28), (int16_t)(cy - 24), color, 4, alpha);

    gfx.popTransform();
}

/* -- Helper: draw steam wisps -------------------------------------------- */
static void drawCoffeeSteam(GfxEngine& gfx, int16_t cx, int16_t topY,
                            float t, float reveal, uint16_t color) {
    for (int i = 0; i < 2; i++) {
        float p = fmodf(t * 0.7f + (float)i * 0.5f, 1.0f);
        int16_t sx = (int16_t)(cx - 8 + i * 16);
        int16_t sy = (int16_t)((float)topY - p * 36.0f);
        uint8_t op = (uint8_t)((1.0f - p) * reveal * 200);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(sx, sy,
                           (int16_t)(sx + 6), (int16_t)(sy - 8),
                           sx, (int16_t)(sy - 16), color, 3);
        gfx.drawQuadBezier(sx, (int16_t)(sy - 16),
                           (int16_t)(sx - 6), (int16_t)(sy - 24),
                           sx, (int16_t)(sy - 32), color, 3);
        gfx.popAlpha();
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_coffee_pour(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, COFFEE_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "sleepy")) {
        eyeEmo = "sleepy";
    } else if (phaseIn(ph.name, "cupIn", "potIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIs(ph.name, "pour")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "potOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "steam")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "sip")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "cupOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Props -------------------------------------------------------- */
    int16_t cupCx = SCX - 20;
    int16_t cupCy = CY + 22;
    int16_t potCx = SCX + 60;
    int16_t potCy = CY - 10;

    // Cup tracking
    bool showCup = false;
    float cupFill = 0.0f;
    int16_t cupDrawX = cupCx, cupDrawY = cupCy;
    uint8_t cupAlpha = 255;
    float cupTilt = 0.0f;

    // Pot tracking
    bool showPot = false;
    int16_t potDrawX = potCx, potDrawY = potCy;
    uint8_t potAlpha = 255;
    float potTilt = 0.0f;

    if (phaseIs(ph.name, "cupIn")) {
        showCup = true;
        float ep = ease::out(ph.p);
        cupDrawY = (int16_t)lerp((float)(SCREEN_H + 30), (float)cupCy, ep);
        cupAlpha = (uint8_t)(ep * 255);
    } else if (phaseIs(ph.name, "potIn")) {
        showCup = true;
        showPot = true;
        float ep = ease::out(ph.p);
        potDrawX = (int16_t)lerp((float)(SCREEN_W + 30), (float)potCx, ep);
        potAlpha = (uint8_t)(ep * 255);
    } else if (phaseIs(ph.name, "shrink")) {
        showCup = true;
        showPot = true;
    } else if (phaseIs(ph.name, "pour")) {
        showCup = true;
        showPot = true;
        cupFill = ease::out(ph.p);
        // Pot tilts during pour
        potTilt = sinf(clamp(ph.p * 2.0f, 0.0f, 1.0f) * PI / 2.0f) * 25.0f;

        // Pour stream (line from spout to cup)
        if (ph.p > 0.05f && ph.p < 0.9f) {
            float streamAlpha = 1.0f;
            if (ph.p > 0.8f) streamAlpha = (0.9f - ph.p) * 10.0f;
            // Approximate spout position after tilt
            int16_t spoutX = (int16_t)(potCx + 20);
            int16_t spoutY = (int16_t)(potCy - 10);
            int16_t cupTop = (int16_t)(cupCy - 4);
            gfx.drawLine(spoutX, spoutY, (int16_t)(cupCx + 4), cupTop,
                         colors.eye, 3, (uint8_t)(streamAlpha * 200));
        }
    } else if (phaseIs(ph.name, "potOut")) {
        showCup = true;
        showPot = true;
        cupFill = 1.0f;
        float ep = ease::in(ph.p);
        potDrawX = (int16_t)lerp((float)potCx, (float)(SCREEN_W + 30), ep);
        potAlpha = (uint8_t)((1.0f - ep) * 255);
    } else if (phaseIs(ph.name, "steam")) {
        showCup = true;
        cupFill = 1.0f;
        drawCoffeeSteam(gfx, cupCx, cupCy - 8, t, ease::out(ph.p), colors.eye);
    } else if (phaseIs(ph.name, "sip")) {
        showCup = true;
        cupFill = 1.0f;
        // Cup tilts for sip
        float sipP = sinf(ph.p * PI);
        cupTilt = sipP * -20.0f;
        cupDrawY = (int16_t)((float)cupCy - sipP * 8.0f);
    } else if (phaseIs(ph.name, "cupOut")) {
        showCup = true;
        cupFill = 1.0f;
        float ep = ease::in(ph.p);
        cupDrawY = (int16_t)lerp((float)cupCy, (float)(SCREEN_H + 30), ep);
        cupAlpha = (uint8_t)((1.0f - ep) * 255);
    }

    // Draw pot (behind cup)
    if (showPot) {
        drawCoffeePot(gfx, potDrawX, potDrawY, potTilt, colors.eye, potAlpha);
    }

    // Draw cup
    if (showCup) {
        if (fabsf(cupTilt) > 0.5f) {
            gfx.pushTransform();
            gfx.rotate(cupTilt, (float)cupDrawX, (float)cupDrawY);
        }
        drawCup(gfx, cupDrawX, cupDrawY, cupFill, colors.eye, colors.bg, cupAlpha);
        if (fabsf(cupTilt) > 0.5f) {
            gfx.popTransform();
        }
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "pour", "potOut")) {
        drawActLabel(gfx, "COFFEE", colors.eye);
    } else if (phaseIn(ph.name, "steam", "sip")) {
        drawActLabel(gfx, "COFFEE TIME", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "REFRESHED", colors.eye);
    }
}

const VariantDef COFFEE_VARIANTS[] = {
    {"coffee-pour", "Coffee Pour", 14000, TONE_NONE, render_coffee_pour},
};

extern const CategoryDef CAT_COFFEE = {
    "coffee", "Coffee", ContentKind::SCENE, TONE_WARM,
    COFFEE_VARIANTS, sizeof(COFFEE_VARIANTS) / sizeof(COFFEE_VARIANTS[0]), false
};
