#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry GIFT_PHASES[] = {
    {"curious",  0.06f},   // Curious Luni, centered
    {"boxIn",    0.07f},   // Wrapped gift box slides in from left
    {"shrink",   0.05f},   // Eyes shrink to corner
    {"shake",    0.10f},   // Box shakes with anticipation
    {"unwrap",   0.12f},   // Wrapping peels off
    {"lidOff",   0.10f},   // Lid lifts upward
    {"reveal",   0.14f},   // Star/heart revealed inside
    {"sparkle",  0.10f},   // Sparkle burst celebration
    {"boxOut",   0.06f},   // Box exits to right
    {"grow",     0.07f},   // Eyes grow back
    {"done",     0.13f},   // Excited Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(GIFT_PHASES) / sizeof(GIFT_PHASES[0]);

/* -- Helper: draw wrapped gift box --------------------------------------- */
static void drawGiftBox(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint8_t alpha) {
    // Box body
    gfx.fillRoundedRect((int16_t)(cx - 44), (int16_t)(cy - 24), 88, 56, 4, color, alpha);

    // Vertical ribbon
    gfx.fillRect((int16_t)(cx - 4), (int16_t)(cy - 24), 8, 56, COLOR_BG, alpha);
    // Horizontal ribbon
    gfx.fillRect((int16_t)(cx - 44), (int16_t)(cy - 2), 88, 6, COLOR_BG, alpha);

    // Ribbon center strips (thinner, eye color on top)
    gfx.fillRect((int16_t)(cx - 2), (int16_t)(cy - 24), 4, 56, color, alpha);
    gfx.fillRect((int16_t)(cx - 44), (int16_t)(cy), 88, 2, color, alpha);

    // Bow loops on top
    gfx.drawQuadBezier(cx, (int16_t)(cy - 24),
                       (int16_t)(cx - 22), (int16_t)(cy - 44),
                       (int16_t)(cx - 10), (int16_t)(cy - 24), color, 3);
    gfx.drawQuadBezier(cx, (int16_t)(cy - 24),
                       (int16_t)(cx + 22), (int16_t)(cy - 44),
                       (int16_t)(cx + 10), (int16_t)(cy - 24), color, 3);
}

/* -- Helper: draw unwrapped box (no ribbons) ----------------------------- */
static void drawOpenBox(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint8_t alpha) {
    // Box body (no ribbons)
    gfx.fillRoundedRect((int16_t)(cx - 44), (int16_t)(cy - 14), 88, 46, 4, color, alpha);
}

/* -- Helper: draw lid ---------------------------------------------------- */
static void drawLid(GfxEngine& gfx, int16_t cx, int16_t cy,
                    float liftY, float angle, uint16_t color, uint8_t alpha) {
    gfx.pushTransform();
    gfx.translate((float)cx, (float)(cy - 14) + liftY);
    gfx.rotate(-angle, -44.0f, 0.0f);
    gfx.fillRoundedRect(-50, -12, 100, 12, 3, color, alpha);
    gfx.popTransform();
}

/* -- Helper: draw star shape --------------------------------------------- */
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

/* -- Helper: draw heart shape -------------------------------------------- */
static void drawHeart(GfxEngine& gfx, int16_t cx, int16_t cy, float s,
                      uint16_t color, uint8_t alpha) {
    float k = s / 30.0f;
    int16_t r = (int16_t)(9.0f * k);
    int16_t bumpY = (int16_t)(cy - 9.0f * k);
    int16_t tipY  = (int16_t)(cy + 9.0f * k);
    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

/* -- Helper: draw sparkle burst ------------------------------------------ */
static void drawSparkles(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float t, uint16_t color) {
    for (int i = 0; i < 8; i++) {
        float angle = (float)i * (TAU / 8.0f) + t * TAU * 0.5f;
        float radius = 30.0f + sinf(t * TAU * 2.0f + (float)i) * 12.0f;
        float sp = fmodf(t * 1.5f + (float)i * 0.125f, 1.0f);
        int16_t sx = (int16_t)(cx + cosf(angle) * radius);
        int16_t sy = (int16_t)(cy + sinf(angle) * radius * 0.7f);
        uint8_t op = (uint8_t)((1.0f - sp) * 220);
        int16_t sr = (int16_t)(2.0f + (1.0f - sp) * 3.0f);
        gfx.fillCircle(sx, sy, sr, color, op);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_gift_unwrap(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, GIFT_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "curious")) {
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
    } else if (phaseIs(ph.name, "unwrap")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "lidOff")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIn(ph.name, "reveal", "sparkle")) {
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

    /* --- Prop: gift box ----------------------------------------------- */
    int16_t boxCx = SCX, boxCy = CY + 12;

    if (phaseIs(ph.name, "boxIn")) {
        // Slide in from left
        float ep = ease::out(ph.p);
        boxCx = (int16_t)lerp(-60.0f, (float)SCX, ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawGiftBox(gfx, boxCx, boxCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawGiftBox(gfx, boxCx, boxCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "shake")) {
        // Box shakes left-right with anticipation
        float shake = sinf(ph.p * TAU * 6.0f) * (3.0f + ph.p * 8.0f);
        drawGiftBox(gfx, (int16_t)((float)boxCx + shake), boxCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "unwrap")) {
        // Ribbons fade away, box transitions to plain
        float ribbonAlpha = 1.0f - ease::out(ph.p);
        // Draw plain box
        drawOpenBox(gfx, boxCx, boxCy, colors.eye, 255);
        // Draw lid on top
        drawLid(gfx, boxCx, boxCy, 0.0f, 0.0f, colors.eye, 255);
        // Overlay fading ribbons
        if (ribbonAlpha > 0.05f) {
            uint8_t rOp = (uint8_t)(ribbonAlpha * 200);
            // Vertical ribbon fading
            gfx.fillRect((int16_t)(boxCx - 4), (int16_t)(boxCy - 24), 8, 56, COLOR_BG, rOp);
            // Horizontal ribbon fading
            gfx.fillRect((int16_t)(boxCx - 44), (int16_t)(boxCy - 2), 88, 6, COLOR_BG, rOp);
            // Bow fading
            gfx.drawQuadBezier(boxCx, (int16_t)(boxCy - 24),
                               (int16_t)(boxCx - 22), (int16_t)(boxCy - 44),
                               (int16_t)(boxCx - 10), (int16_t)(boxCy - 24),
                               colors.eye, 3);
            gfx.drawQuadBezier(boxCx, (int16_t)(boxCy - 24),
                               (int16_t)(boxCx + 22), (int16_t)(boxCy - 44),
                               (int16_t)(boxCx + 10), (int16_t)(boxCy - 24),
                               colors.eye, 3);
        }
    } else if (phaseIs(ph.name, "lidOff")) {
        // Lid lifts and rotates off
        float ep = ease::out(ph.p);
        float liftY = -ep * 40.0f;
        float angle = ep * 25.0f;
        drawOpenBox(gfx, boxCx, boxCy, colors.eye, 255);
        drawLid(gfx, boxCx, boxCy, liftY, angle, colors.eye, (uint8_t)((1.0f - ep * 0.3f) * 255));
    } else if (phaseIs(ph.name, "reveal")) {
        // Box open, star/heart rises out
        drawOpenBox(gfx, boxCx, boxCy, colors.eye, 255);
        float riseP = ease::out(ph.p);
        int16_t itemY = (int16_t)lerp((float)(boxCy - 6), (float)(boxCy - 50), riseP);
        float itemR = lerp(6.0f, 20.0f, riseP);
        uint8_t itemOp = (uint8_t)(riseP * 255);
        drawStar(gfx, boxCx, itemY, itemR, colors.eye, itemOp);
        // Small heart next to star
        if (riseP > 0.4f) {
            float heartP = (riseP - 0.4f) / 0.6f;
            drawHeart(gfx, (int16_t)(boxCx + 30), (int16_t)(itemY + 6),
                      10.0f * heartP, colors.eye, (uint8_t)(heartP * 200));
        }
    } else if (phaseIs(ph.name, "sparkle")) {
        // Box open, item floats, sparkles burst
        drawOpenBox(gfx, boxCx, boxCy, colors.eye, 255);
        float bob = sinf(ph.p * TAU * 2.0f) * 4.0f;
        int16_t itemY = (int16_t)(boxCy - 50 + bob);
        drawStar(gfx, boxCx, itemY, 20.0f, colors.eye, 255);
        drawHeart(gfx, (int16_t)(boxCx + 30), (int16_t)(itemY + 6), 10.0f, colors.eye, 200);
        drawSparkles(gfx, boxCx, itemY, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "boxOut")) {
        // Box exits to right
        float ep = ease::in(ph.p);
        boxCx = (int16_t)lerp((float)SCX, (float)(SCREEN_W + 80), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawOpenBox(gfx, boxCx, boxCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "shake", "unwrap")) {
        drawActLabel(gfx, "GIFT", colors.eye);
    } else if (phaseIn(ph.name, "lidOff", "reveal")) {
        drawActLabel(gfx, "SURPRISE!", colors.eye);
    } else if (phaseIs(ph.name, "sparkle")) {
        drawActLabel(gfx, "FOR YOU", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "THANK YOU", colors.eye);
    }
}

const VariantDef GIFT_VARIANTS[] = {
    {"gift-unwrap", "Unwrap", 14000, TONE_NONE, render_gift_unwrap},
};

extern const CategoryDef CAT_GIFT = {
    "gift", "Gift", ContentKind::SCENE, TONE_ROSE,
    GIFT_VARIANTS, sizeof(GIFT_VARIANTS) / sizeof(GIFT_VARIANTS[0]), false
};
