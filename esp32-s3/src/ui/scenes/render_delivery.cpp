#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry DELIVERY_PHASES[] = {
    {"idle",      0.06f},   // Luni curious, centered
    {"boxIn",     0.08f},   // Cardboard box slides in from left
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"shake",     0.08f},   // Box shakes with anticipation
    {"open",      0.10f},   // Top flap opens (rotates up)
    {"reveal",    0.14f},   // Star item revealed inside
    {"celebrate", 0.12f},   // Sparkle celebration
    {"boxOut",    0.07f},   // Box exits to right
    {"grow",      0.07f},   // Eyes grow back to center
    {"done",      0.23f},   // Excited Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(DELIVERY_PHASES) / sizeof(DELIVERY_PHASES[0]);

/* -- Helper: draw box body ----------------------------------------------- */
static void drawBox(GfxEngine& gfx, int16_t cx, int16_t cy,
                    uint16_t color, uint8_t alpha) {
    // Main body
    gfx.fillRoundedRect((int16_t)(cx - 44), (int16_t)(cy - 30), 88, 60, 4, color, alpha);
    // Tape line across middle (darker)
    gfx.drawLine((int16_t)(cx - 44), cy, (int16_t)(cx + 44), cy, color, 2, (uint8_t)(alpha * 0.5f));
    // Vertical tape strip
    gfx.fillRect((int16_t)(cx - 4), (int16_t)(cy - 30), 8, 60, color, (uint8_t)(alpha * 0.5f));
}

/* -- Helper: draw box with flap ------------------------------------------ */
static void drawBoxWithFlap(GfxEngine& gfx, int16_t cx, int16_t cy,
                            float flapAngle, uint16_t color, uint8_t alpha) {
    // Main body (slightly shorter to leave room for flap)
    gfx.fillRoundedRect((int16_t)(cx - 44), (int16_t)(cy - 20), 88, 50, 4, color, alpha);
    // Tape line
    gfx.drawLine((int16_t)(cx - 44), (int16_t)(cy + 5),
                 (int16_t)(cx + 44), (int16_t)(cy + 5), color, 2, (uint8_t)(alpha * 0.5f));

    // Flap (rotates upward from the top edge of the box)
    gfx.pushTransform();
    gfx.translate((float)cx, (float)(cy - 20));
    gfx.rotate(-flapAngle, 0.0f, 0.0f);
    gfx.fillRoundedRect(-44, -10, 88, 10, 2, color, alpha);
    gfx.popTransform();
}

/* -- Helper: draw star --------------------------------------------------- */
static void drawStar(GfxEngine& gfx, int16_t cx, int16_t cy, float r,
                     uint16_t color, uint8_t alpha) {
    for (int i = 0; i < 5; i++) {
        float a1 = -1.5708f + (float)i * (TAU / 5.0f);
        float a2 = a1 + TAU / 10.0f;
        float a3 = a1 + TAU / 5.0f;
        float r2 = r * 0.4f;
        int16_t x0 = (int16_t)(cx + cosf(a1) * r);
        int16_t y0 = (int16_t)(cy + sinf(a1) * r);
        int16_t x1 = (int16_t)(cx + cosf(a2) * r2);
        int16_t y1 = (int16_t)(cy + sinf(a2) * r2);
        int16_t x2 = (int16_t)(cx + cosf(a3) * r);
        int16_t y2 = (int16_t)(cy + sinf(a3) * r);
        gfx.fillTriangle(x0, y0, x1, y1, cx, cy, color, alpha);
        gfx.fillTriangle(x1, y1, x2, y2, cx, cy, color, alpha);
    }
}

/* -- Helper: draw sparkles ----------------------------------------------- */
static void drawSparkles(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float t, uint16_t color) {
    for (int i = 0; i < 8; i++) {
        float angle = (float)i * (TAU / 8.0f) + t * TAU * 0.5f;
        float radius = 30.0f + sinf(t * TAU * 2.0f + (float)i) * 10.0f;
        float sp = fmodf(t * 1.5f + (float)i * 0.125f, 1.0f);
        int16_t sx = (int16_t)(cx + cosf(angle) * radius);
        int16_t sy = (int16_t)(cy + sinf(angle) * radius * 0.7f);
        uint8_t op = (uint8_t)((1.0f - sp) * 220);
        int16_t sr = (int16_t)(2.0f + (1.0f - sp) * 3.0f);
        gfx.fillCircle(sx, sy, sr, color, op);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_delivery(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, DELIVERY_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "boxIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIs(ph.name, "shake")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "open")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIn(ph.name, "reveal", "celebrate")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "boxOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "excited";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "excited";
    }

    /* --- Prop: box ---------------------------------------------------- */
    int16_t boxCx = SCX, boxCy = CY + 10;

    if (phaseIs(ph.name, "boxIn")) {
        // Slide in from left
        float ep = ease::out(ph.p);
        boxCx = (int16_t)lerp(-60.0f, (float)SCX, ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawBox(gfx, boxCx, boxCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawBox(gfx, boxCx, boxCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "shake")) {
        // Box shakes left-right
        float shake = sinf(ph.p * TAU * 6.0f) * (3.0f + ph.p * 5.0f);
        drawBox(gfx, (int16_t)(boxCx + (int16_t)shake), boxCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "open")) {
        // Top flap rotates open
        float flapAngle = ease::out(ph.p) * 80.0f;
        drawBoxWithFlap(gfx, boxCx, boxCy, flapAngle, colors.eye, 255);
    } else if (phaseIs(ph.name, "reveal")) {
        // Box open, star rises out
        drawBoxWithFlap(gfx, boxCx, boxCy, 80.0f, colors.eye, 255);
        float riseP = ease::out(ph.p);
        int16_t starY = (int16_t)lerp((float)(boxCy - 10), (float)(boxCy - 50), riseP);
        float starR = lerp(6.0f, 18.0f, riseP);
        drawStar(gfx, boxCx, starY, starR, colors.eye, (uint8_t)(riseP * 255));
    } else if (phaseIs(ph.name, "celebrate")) {
        // Box open, star floats, sparkles around it
        drawBoxWithFlap(gfx, boxCx, boxCy, 80.0f, colors.eye, 255);
        float bob = sinf(ph.p * TAU * 2.0f) * 4.0f;
        int16_t starY = (int16_t)(boxCy - 50 + bob);
        drawStar(gfx, boxCx, starY, 18.0f, colors.eye, 255);
        drawSparkles(gfx, boxCx, starY, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "boxOut")) {
        // Box exits to right
        float ep = ease::in(ph.p);
        boxCx = (int16_t)lerp((float)SCX, (float)(SCREEN_W + 80), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawBoxWithFlap(gfx, boxCx, boxCy, 80.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_WARM], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "shake", "open")) {
        drawActLabel(gfx, "DELIVERY", colors.eye);
    } else if (phaseIn(ph.name, "reveal", "celebrate")) {
        drawActLabel(gfx, "SURPRISE!", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "UNBOXED", colors.eye);
    }
}

const VariantDef DELIVERY_VARIANTS[] = {
    {"delivery-main", "Delivery", 14000, TONE_NONE, render_delivery},
};

extern const CategoryDef CAT_DELIVERY = {
    "delivery", "Delivery", ContentKind::SCENE, TONE_WARM,
    DELIVERY_VARIANTS, sizeof(DELIVERY_VARIANTS) / sizeof(DELIVERY_VARIANTS[0]), false
};
