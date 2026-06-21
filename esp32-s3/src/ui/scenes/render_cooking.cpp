#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry COOKING_PHASES[] = {
    {"hungry",  0.06f},   // Luni hungry (eager), centered
    {"potIn",   0.07f},   // Pot slides in from bottom
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"toss1",   0.08f},   // Ingredient 1 arcs into pot
    {"toss2",   0.08f},   // Ingredient 2 arcs into pot
    {"toss3",   0.08f},   // Ingredient 3 arcs into pot
    {"stir",    0.14f},   // Spoon stirs with bubbles
    {"steam",   0.10f},   // Steam rises from pot
    {"plate",   0.08f},   // Plate slides in from right
    {"potOut",  0.06f},   // Pot and plate exit
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.13f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(COOKING_PHASES) / sizeof(COOKING_PHASES[0]);

/* -- Helper: draw pot ---------------------------------------------------- */
static void drawPot(GfxEngine& gfx, int16_t cx, int16_t cy,
                    uint16_t color, uint8_t alpha) {
    // Pot body (rounded rect)
    gfx.fillRoundedRect((int16_t)(cx - 46), (int16_t)(cy - 10),
                        92, 44, 6, color, alpha);
    // Rim (wider bar at top)
    gfx.fillRect((int16_t)(cx - 52), (int16_t)(cy - 14), 104, 8, color, alpha);
    // Handles
    gfx.drawLine((int16_t)(cx - 52), (int16_t)(cy - 8),
                 (int16_t)(cx - 62), (int16_t)(cy - 2), color, 5, alpha);
    gfx.drawLine((int16_t)(cx + 52), (int16_t)(cy - 8),
                 (int16_t)(cx + 62), (int16_t)(cy - 2), color, 5, alpha);
}

/* -- Helper: draw ingredient arc ----------------------------------------- */
static void drawIngredientArc(GfxEngine& gfx, float p, int16_t potCx,
                              int16_t potTop, int idx,
                              uint16_t color, uint8_t alpha) {
    // Start from upper-right, arc into pot
    float startX = 280.0f - (float)idx * 20.0f;
    float startY = 40.0f + (float)idx * 15.0f;
    float endX   = (float)potCx + (float)(idx - 1) * 14.0f;
    float endY   = (float)potTop;

    float ep = ease::out(p);
    // Parabolic arc: x lerps, y follows arc
    float cx = lerp(startX, endX, ep);
    float cy = lerp(startY, endY, ep) - sinf(ep * PI) * 50.0f;

    int16_t r = (int16_t)(6 - idx);
    if (ep < 0.95f) {
        gfx.fillCircle((int16_t)cx, (int16_t)cy, r, color, alpha);
    }
}

/* -- Helper: draw bubbles ------------------------------------------------ */
static void drawBubbles(GfxEngine& gfx, int16_t cx, int16_t potTop,
                        float t, uint16_t color) {
    for (int i = 0; i < 5; i++) {
        float p = fmodf(t * 2.0f + (float)i * 0.2f, 1.0f);
        int16_t bx = (int16_t)(cx - 30 + i * 15);
        int16_t by = (int16_t)((float)potTop + 8.0f - p * 20.0f);
        int16_t br = (int16_t)(3.0f + (1.0f - p) * 2.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 200);
        gfx.fillCircle(bx, by, br, color, op);
    }
}

/* -- Helper: draw steam wisps -------------------------------------------- */
static void drawSteam(GfxEngine& gfx, int16_t cx, int16_t topY,
                      float t, float reveal, uint16_t color) {
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.8f + (float)i * 0.33f, 1.0f);
        int16_t sx = (int16_t)(cx - 20 + i * 20);
        int16_t sy = (int16_t)((float)topY - p * 50.0f);
        uint8_t op = (uint8_t)((1.0f - p) * reveal * 200);
        gfx.pushAlpha(op);
        gfx.drawQuadBezier(sx, sy,
                           (int16_t)(sx + 6), (int16_t)(sy - 10),
                           sx, (int16_t)(sy - 20), color, 3);
        gfx.drawQuadBezier(sx, (int16_t)(sy - 20),
                           (int16_t)(sx - 6), (int16_t)(sy - 28),
                           sx, (int16_t)(sy - 36), color, 3);
        gfx.popAlpha();
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_cooking_pot(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, COOKING_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "hungry")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "potIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "toss1", "toss2", "toss3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "stir")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIn(ph.name, "steam", "plate")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "potOut")) {
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

    /* --- Pot props ---------------------------------------------------- */
    int16_t potCx = SCX;
    int16_t potCy = CY + 28;
    int16_t potTop = potCy - 14;

    if (phaseIs(ph.name, "potIn")) {
        // Pot slides up from bottom
        float ep = ease::out(ph.p);
        int16_t fromY = SCREEN_H + 40;
        int16_t curY = (int16_t)lerp((float)fromY, (float)potCy, ep);
        drawPot(gfx, potCx, curY, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "toss1")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
        drawIngredientArc(gfx, ph.p, potCx, potTop, 0, colors.eye, 255);
    } else if (phaseIs(ph.name, "toss2")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
        drawIngredientArc(gfx, ph.p, potCx, potTop, 1, colors.eye, 255);
    } else if (phaseIs(ph.name, "toss3")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
        drawIngredientArc(gfx, ph.p, potCx, potTop, 2, colors.eye, 255);
    } else if (phaseIs(ph.name, "stir")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);

        // Rotating spoon
        float angle = ph.p * TAU * 3.0f;
        int16_t spoonX = (int16_t)((float)potCx + cosf(angle) * 18.0f);
        int16_t spoonY = (int16_t)((float)potTop + 4.0f + sinf(angle) * 8.0f);
        gfx.drawLine(spoonX, spoonY,
                     (int16_t)(spoonX - cosf(angle) * 30.0f),
                     (int16_t)(spoonY - 40.0f),
                     colors.eye, 4);
        // Spoon head (circle at bottom of line)
        gfx.fillCircle(spoonX, spoonY, 6, colors.eye);

        // Bubbles
        drawBubbles(gfx, potCx, potTop, t, colors.eye);
    } else if (phaseIs(ph.name, "steam")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
        drawBubbles(gfx, potCx, potTop, t, colors.eye);
        drawSteam(gfx, potCx, potTop, t, ease::out(ph.p), colors.eye);
    } else if (phaseIs(ph.name, "plate")) {
        drawPot(gfx, potCx, potCy, colors.eye, 255);
        drawSteam(gfx, potCx, potTop, t, 1.0f, colors.eye);

        // Plate slides in from right
        float ep = ease::out(ph.p);
        int16_t plateX = (int16_t)lerp((float)(SCREEN_W + 40), (float)(potCx + 90), ep);
        int16_t plateY = potCy + 30;
        gfx.fillEllipse(plateX, plateY, 28, 8, colors.eye, (uint8_t)(ep * 255));
        gfx.fillEllipse(plateX, (int16_t)(plateY - 4), 20, 4, colors.bg,
                        (uint8_t)(ep * 200));
    } else if (phaseIs(ph.name, "potOut")) {
        // Pot and plate exit downward
        float ep = ease::in(ph.p);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        int16_t exitY = (int16_t)lerp((float)potCy, (float)(SCREEN_H + 40), ep);
        drawPot(gfx, potCx, exitY, colors.eye, alpha);

        int16_t plateX = potCx + 90;
        int16_t plateY = (int16_t)lerp((float)(potCy + 30), (float)(SCREEN_H + 40), ep);
        gfx.fillEllipse(plateX, plateY, 28, 8, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "toss1", "toss2", "toss3", "stir")) {
        drawActLabel(gfx, "COOKING", colors.eye);
    } else if (phaseIn(ph.name, "steam", "plate")) {
        drawActLabel(gfx, "READY!", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "BON APPETIT", colors.eye);
    }
}

const VariantDef COOKING_VARIANTS[] = {
    {"cooking-pot", "Cooking Pot", 14000, TONE_NONE, render_cooking_pot},
};

extern const CategoryDef CAT_COOKING = {
    "cooking", "Cooking", ContentKind::SCENE, TONE_WARM,
    COOKING_VARIANTS, sizeof(COOKING_VARIANTS) / sizeof(COOKING_VARIANTS[0]), false
};
