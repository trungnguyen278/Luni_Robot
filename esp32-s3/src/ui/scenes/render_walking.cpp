#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry WALK_PHASES[] = {
    {"eager",   0.06f},   // Luni eager, centered
    {"suitUp",  0.06f},   // Preparation anticipation
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"walk",    0.36f},   // Stick figure walks with scrolling scenery
    {"goal",    0.10f},   // Finish flag enters
    {"cheer",   0.10f},   // Confetti burst celebration
    {"exit",    0.06f},   // Figure exits right
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.14f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(WALK_PHASES) / sizeof(WALK_PHASES[0]);

/* -- Helper: draw stick figure ------------------------------------------- */
static void drawStickFigure(GfxEngine& gfx, int16_t cx, int16_t cy,
                            float legPhase, uint16_t color, uint8_t alpha) {
    // Head
    gfx.fillCircle(cx, (int16_t)(cy - 28), 8, color, alpha);
    // Body
    gfx.drawLine(cx, (int16_t)(cy - 20), cx, (int16_t)(cy - 2),
                 color, 3, alpha);
    // Arms swing
    float armA = sinf(legPhase) * 22.0f;
    gfx.drawLine(cx, (int16_t)(cy - 16),
                 (int16_t)(cx - 10.0f + armA * 0.5f), (int16_t)(cy - 6),
                 color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 16),
                 (int16_t)(cx + 10.0f - armA * 0.5f), (int16_t)(cy - 6),
                 color, 3, alpha);
    // Legs animate
    float legA = sinf(legPhase) * 18.0f;
    gfx.drawLine(cx, (int16_t)(cy - 2),
                 (int16_t)(cx - 8.0f + legA), cy,
                 color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 2),
                 (int16_t)(cx + 8.0f - legA), cy,
                 color, 3, alpha);
}

/* -- Helper: draw tree (triangle on stick) ------------------------------- */
static void drawTree(GfxEngine& gfx, int16_t x, int16_t groundY,
                     uint16_t color, uint8_t alpha) {
    // Trunk
    gfx.fillRect((int16_t)(x - 2), (int16_t)(groundY - 20), 4, 20, color, alpha);
    // Canopy (triangle)
    gfx.fillTriangle((int16_t)(x - 14), (int16_t)(groundY - 20),
                     (int16_t)(x + 14), (int16_t)(groundY - 20),
                     x, (int16_t)(groundY - 48), color, alpha);
}

/* -- Helper: draw finish flag -------------------------------------------- */
static void drawFlag(GfxEngine& gfx, int16_t x, int16_t groundY,
                     uint16_t color, uint8_t alpha) {
    // Pole
    gfx.drawLine(x, groundY, x, (int16_t)(groundY - 44), color, 2, alpha);
    // Flag rect
    gfx.fillRect(x, (int16_t)(groundY - 44), 18, 12, color, alpha);
    // Checker pattern (2 small dark rects)
    gfx.fillRect((int16_t)(x + 9), (int16_t)(groundY - 44), 9, 6, COLOR_BG, (uint8_t)(alpha * 0.5f));
    gfx.fillRect(x, (int16_t)(groundY - 38), 9, 6, COLOR_BG, (uint8_t)(alpha * 0.5f));
}

/* -- Main render --------------------------------------------------------- */
static void render_walking(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, WALK_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "eager")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "suitUp")) {
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "excited";
    } else if (phaseIn(ph.name, "walk", "goal", "cheer", "exit")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        if (phaseIs(ph.name, "walk"))  eyeEmo = "eager";
        else if (phaseIs(ph.name, "goal"))  eyeEmo = "surprised";
        else if (phaseIs(ph.name, "cheer")) eyeEmo = "excited";
        else eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "happy";
    }

    /* --- Scene props -------------------------------------------------- */
    int16_t groundY = SCREEN_H - 28;

    if (phaseIs(ph.name, "suitUp")) {
        // Pulsing dots (anticipation)
        for (int i = 0; i < 3; i++) {
            float dp = sinf(ph.p * TAU * 2.0f + (float)i * 1.2f);
            uint8_t op = (uint8_t)(clamp(dp, 0.0f, 1.0f) * 200);
            gfx.fillCircle((int16_t)(SCX - 20 + i * 20), CY + 30, 4, colors.eye, op);
        }
    } else if (phaseIs(ph.name, "shrink")) {
        // Ground appears
        gfx.drawLine(0, groundY, SCREEN_W, groundY, colors.eye, 2, (uint8_t)(ph.p * 255));
    } else if (phaseIs(ph.name, "walk")) {
        // Scrolling ground line
        gfx.drawLine(0, groundY, SCREEN_W, groundY, colors.eye, 2);

        // Scrolling trees (3 trees cycling across screen)
        float scroll = ph.p * 600.0f;
        for (int i = 0; i < 4; i++) {
            float tx = fmodf(340.0f - scroll + (float)i * 120.0f, 480.0f) - 80.0f;
            if (tx > -30.0f && tx < (float)(SCREEN_W + 30)) {
                drawTree(gfx, (int16_t)tx, groundY, colors.eye, 180);
            }
        }

        // Walking stick figure (center-left, bobbing)
        float bob = fabsf(sinf(ph.p * TAU * 12.0f)) * 3.0f;
        int16_t figX = SCX - 40;
        int16_t figY = (int16_t)(groundY - bob);
        drawStickFigure(gfx, figX, figY, ph.p * TAU * 12.0f, colors.eye, 255);

        // Step counter (top-left area, increments)
        int steps = (int)(ph.p * 4800.0f) + 200;
        char stepBuf[12];
        snprintf(stepBuf, sizeof(stepBuf), "%d", steps);
        gfx.drawText(stepBuf, 50, STATUS_H + 36, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("STEPS", 50, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);

        // Speed lines behind figure
        for (int i = 0; i < 3; i++) {
            float lp = fmodf(ph.p * 8.0f + (float)i * 0.33f, 1.0f);
            gfx.drawLine((int16_t)(figX - 22.0f - lp * 20.0f), (int16_t)(figY - 20 + i * 8),
                         (int16_t)(figX - 12.0f - lp * 10.0f), (int16_t)(figY - 20 + i * 8),
                         colors.eye, 2, (uint8_t)((1.0f - lp) * 150));
        }
    } else if (phaseIs(ph.name, "goal")) {
        // Ground + figure stopped + flag enters from right
        gfx.drawLine(0, groundY, SCREEN_W, groundY, colors.eye, 2);

        int16_t figX = SCX - 40;
        drawStickFigure(gfx, figX, groundY, 0.0f, colors.eye, 255);

        // Step counter final
        gfx.drawText("5000", 50, STATUS_H + 36, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("STEPS", 50, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);

        // Flag slides in from right
        float ep = ease::out(ph.p);
        int16_t flagX = (int16_t)lerp((float)(SCREEN_W + 30), (float)(SCX + 50), ep);
        drawFlag(gfx, flagX, groundY, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "cheer")) {
        // Ground + figure + flag + confetti
        gfx.drawLine(0, groundY, SCREEN_W, groundY, colors.eye, 2);

        int16_t figX = SCX - 40;
        // Figure with arms up (celebration pose)
        gfx.fillCircle(figX, (int16_t)(groundY - 28), 8, colors.eye);
        gfx.drawLine(figX, (int16_t)(groundY - 20), figX, (int16_t)(groundY - 2),
                     colors.eye, 3);
        // Arms raised
        gfx.drawLine(figX, (int16_t)(groundY - 16),
                     (int16_t)(figX - 14), (int16_t)(groundY - 30),
                     colors.eye, 3);
        gfx.drawLine(figX, (int16_t)(groundY - 16),
                     (int16_t)(figX + 14), (int16_t)(groundY - 30),
                     colors.eye, 3);
        // Legs standing
        gfx.drawLine(figX, (int16_t)(groundY - 2),
                     (int16_t)(figX - 8), groundY, colors.eye, 3);
        gfx.drawLine(figX, (int16_t)(groundY - 2),
                     (int16_t)(figX + 8), groundY, colors.eye, 3);

        drawFlag(gfx, SCX + 50, groundY, colors.eye, 255);

        // Confetti burst
        static const ToneId CONF_TONES[] = {
            TONE_WARM, TONE_ROSE, TONE_CYAN, TONE_GREEN, TONE_PURPLE, TONE_ORANGE
        };
        for (int i = 0; i < 14; i++) {
            float ip = fmodf(ph.p * 1.5f + (float)i * 0.07f, 1.0f);
            int16_t x = (int16_t)((i * 47 + (int)(sinf(ip * TAU + (float)i) * 8.0f)) % SCREEN_W);
            int16_t y = (int16_t)lerp((float)(STATUS_H + 10), (float)(SCREEN_H - 10), ip);
            uint8_t op = (uint8_t)((1.0f - ip) * 220);
            uint16_t pc = colors.tones[CONF_TONES[i % 6]];
            if (i % 2 == 0) {
                gfx.fillRect((int16_t)(x - 3), (int16_t)(y - 2), 6, 4, pc, op);
            } else {
                gfx.fillCircle(x, y, 3, pc, op);
            }
        }
    } else if (phaseIs(ph.name, "exit")) {
        // Figure walks off right, ground fades
        gfx.drawLine(0, groundY, SCREEN_W, groundY, colors.eye, 2,
                     (uint8_t)((1.0f - ph.p) * 255));

        float ep = ease::in(ph.p);
        int16_t figX = (int16_t)lerp((float)(SCX - 40), (float)(SCREEN_W + 30), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawStickFigure(gfx, figX, groundY, ph.p * TAU * 6.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_GREEN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIs(ph.name, "walk")) {
        drawActLabel(gfx, "WALKING", colors.eye);
    } else if (phaseIn(ph.name, "goal", "cheer")) {
        drawActLabel(gfx, "GOAL!", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "WELL DONE", colors.eye);
    }
}

const VariantDef WALKING_VARIANTS[] = {
    {"walking-scroll", "Walk Scroll", 14000, TONE_NONE, render_walking},
};

extern const CategoryDef CAT_WALKING = {
    "walking", "Walking", ContentKind::SCENE, TONE_GREEN,
    WALKING_VARIANTS, sizeof(WALKING_VARIANTS) / sizeof(WALKING_VARIANTS[0]), false
};
