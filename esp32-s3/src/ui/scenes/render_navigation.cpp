#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry NAV_PHASES[] = {
    {"plan",      0.06f},   // Luni thinking, rotating circle
    {"start",     0.06f},   // Navigation starts
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"straight",  0.12f},   // Arrow points up (go straight)
    {"left",      0.10f},   // Arrow rotates left
    {"straight2", 0.12f},   // Arrow points up again
    {"right",     0.10f},   // Arrow rotates right
    {"arrive",    0.10f},   // Arrow becomes pin marker
    {"exit",      0.06f},   // Pin exits upward
    {"grow",      0.07f},   // Eyes grow back to center
    {"done",      0.16f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(NAV_PHASES) / sizeof(NAV_PHASES[0]);

/* -- Helper: draw direction arrow ---------------------------------------- */
static void drawNavArrow(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float angle, float scale, uint16_t color, uint8_t alpha) {
    gfx.pushTransform();
    gfx.translate((float)cx, (float)cy);
    gfx.rotate(angle, 0, 0);
    gfx.scale(scale, scale);
    // Arrow pointing up: triangle tip + shaft
    gfx.fillTriangle(0, -32, -18, 4, 18, 4, color, alpha);
    gfx.fillRoundedRect(-6, 4, 12, 28, 3, color, alpha);
    gfx.popTransform();
}

/* -- Helper: draw pin marker ---------------------------------------------- */
static void drawPinMarker(GfxEngine& gfx, int16_t cx, int16_t cy,
                          uint16_t color, uint16_t bgColor, uint8_t alpha) {
    // Teardrop: circle top + triangle bottom
    gfx.fillCircle(cx, (int16_t)(cy - 14), 16, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 14), (int16_t)(cy - 10),
                     (int16_t)(cx + 14), (int16_t)(cy - 10),
                     cx, (int16_t)(cy + 14), color, alpha);
    // Inner circle (bg color)
    gfx.fillCircle(cx, (int16_t)(cy - 14), 7, bgColor, alpha);
}

/* -- Helper: draw pulsing rings ------------------------------------------ */
static void drawPulseRings(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float t, uint16_t color) {
    for (int w = 0; w < 2; w++) {
        float p = fmodf(t * 1.5f + (float)w * 0.5f, 1.0f);
        int16_t rr = (int16_t)lerp(8.0f, 50.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 140);
        gfx.drawArc(cx, cy, rr, 0, TAU, color, 2, op);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_navigation(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, NAV_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "thinking";

    if (phaseIs(ph.name, "plan")) {
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "start")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "straight", "left", "straight2", "right")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "arrive")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "exit")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Scene props -------------------------------------------------- */
    int16_t arrowCx = SCX, arrowCy = CY + 6;

    if (phaseIs(ph.name, "plan")) {
        // Rotating planning circle with tick marks
        float rot = ph.p * TAU * 1.5f;
        gfx.pushTransform();
        gfx.translate((float)SCX, (float)CY);
        gfx.rotate(rot * 180.0f / PI, 0, 0);
        gfx.strokeCircle(0, 0, 40, colors.eye, 2);
        for (int i = 0; i < 4; i++) {
            float a = (float)i / 4.0f * TAU;
            gfx.drawLine((int16_t)(cosf(a) * 34.0f), (int16_t)(sinf(a) * 34.0f),
                         (int16_t)(cosf(a) * 40.0f), (int16_t)(sinf(a) * 40.0f),
                         colors.eye, 2);
        }
        gfx.popTransform();
    } else if (phaseIs(ph.name, "start")) {
        // Arrow appears (fade in pointing up)
        uint8_t alpha = (uint8_t)(ease::out(ph.p) * 255);
        drawNavArrow(gfx, arrowCx, arrowCy, 0.0f, 1.0f, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawNavArrow(gfx, arrowCx, arrowCy, 0.0f, 1.0f, colors.eye, 255);
    } else if (phaseIs(ph.name, "straight")) {
        // Arrow pointing up, distance counting down
        drawNavArrow(gfx, arrowCx, arrowCy, 0.0f, 1.0f, colors.eye, 255);

        int dist = 500 - (int)(ph.p * 200.0f);
        char distBuf[16];
        snprintf(distBuf, sizeof(distBuf), "%d M", dist);
        gfx.drawText(distBuf, SCX, SCREEN_H - 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
        gfx.drawText("GO STRAIGHT", SCX, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
    } else if (phaseIs(ph.name, "left")) {
        // Arrow rotates to point left (90 deg counterclockwise = -90)
        float ep = ease::inOut(clamp(ph.p / 0.4f, 0.0f, 1.0f));
        float angle = lerp(0.0f, -90.0f, ep);
        drawNavArrow(gfx, arrowCx, arrowCy, angle, 1.0f, colors.eye, 255);

        gfx.drawText("TURN LEFT", SCX, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
        int dist = 300 - (int)(ph.p * 100.0f);
        char distBuf[16];
        snprintf(distBuf, sizeof(distBuf), "%d M", dist);
        gfx.drawText(distBuf, SCX, SCREEN_H - 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
    } else if (phaseIs(ph.name, "straight2")) {
        // Arrow rotates back to pointing up
        float ep = ease::inOut(clamp(ph.p / 0.3f, 0.0f, 1.0f));
        float angle = lerp(-90.0f, 0.0f, ep);
        drawNavArrow(gfx, arrowCx, arrowCy, angle, 1.0f, colors.eye, 255);

        gfx.drawText("GO STRAIGHT", SCX, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
        int dist = 200 - (int)(ph.p * 120.0f);
        char distBuf[16];
        snprintf(distBuf, sizeof(distBuf), "%d M", dist);
        gfx.drawText(distBuf, SCX, SCREEN_H - 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
    } else if (phaseIs(ph.name, "right")) {
        // Arrow rotates to point right (90 deg clockwise)
        float ep = ease::inOut(clamp(ph.p / 0.4f, 0.0f, 1.0f));
        float angle = lerp(0.0f, 90.0f, ep);
        drawNavArrow(gfx, arrowCx, arrowCy, angle, 1.0f, colors.eye, 255);

        gfx.drawText("TURN RIGHT", SCX, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
        int dist = 80 - (int)(ph.p * 60.0f);
        char distBuf[16];
        snprintf(distBuf, sizeof(distBuf), "%d M", dist);
        gfx.drawText(distBuf, SCX, SCREEN_H - 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 200);
    } else if (phaseIs(ph.name, "arrive")) {
        // Arrow morphs into pin marker (scale down arrow, scale up pin)
        float ep = ease::inOut(clamp(ph.p / 0.4f, 0.0f, 1.0f));
        if (ep < 1.0f) {
            float arrowScale = 1.0f - ep;
            drawNavArrow(gfx, arrowCx, arrowCy, 0.0f, arrowScale, colors.eye,
                         (uint8_t)((1.0f - ep) * 255));
        }
        if (ep > 0.0f) {
            uint8_t pinAlpha = (uint8_t)(ep * 255);
            drawPinMarker(gfx, arrowCx, arrowCy, colors.eye, colors.bg, pinAlpha);
        }
        // Pulsing rings around arrival point
        if (ph.p > 0.4f) {
            drawPulseRings(gfx, arrowCx, arrowCy, (ph.p - 0.4f) / 0.6f, colors.eye);
        }

        gfx.drawText("ARRIVED!", SCX, STATUS_H + 28, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
    } else if (phaseIs(ph.name, "exit")) {
        // Pin exits upward
        float ep = ease::in(ph.p);
        int16_t pinY = (int16_t)lerp((float)arrowCy, (float)(STATUS_H - 50), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawPinMarker(gfx, arrowCx, pinY, colors.eye, colors.bg, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "straight", "left", "straight2", "right")) {
        drawActLabel(gfx, "NAVIGATING", colors.eye);
    } else if (phaseIs(ph.name, "arrive")) {
        drawActLabel(gfx, "DESTINATION", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "YOU MADE IT", colors.eye);
    }
}

const VariantDef NAVIGATION_VARIANTS[] = {
    {"nav-turn", "Nav Turn", 14000, TONE_NONE, render_navigation},
};

extern const CategoryDef CAT_NAVIGATION = {
    "navigation", "Navigation", ContentKind::SCENE, TONE_CYAN,
    NAVIGATION_VARIANTS, sizeof(NAVIGATION_VARIANTS) / sizeof(NAVIGATION_VARIANTS[0]), false
};
