#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry FIT_PHASES[] = {
    {"lazy",    0.05f},   // Luni sleepy
    {"suitUp",  0.05f},   // Prep anticipation
    {"millIn",  0.06f},   // Treadmill rises from bottom
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"warmup",  0.10f},   // Slow running figure on treadmill
    {"run",     0.16f},   // Faster, HR display climbing
    {"sprint",  0.14f},   // Fastest, sweat drops
    {"cooldown",0.08f},   // Slow down
    {"fist",    0.06f},   // Fist pump celebration
    {"millOut", 0.06f},   // Treadmill exits downward
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.12f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(FIT_PHASES) / sizeof(FIT_PHASES[0]);

/* -- Helper: draw treadmill ---------------------------------------------- */
static void drawTreadmill(GfxEngine& gfx, int16_t cx, int16_t cy,
                          uint16_t color, uint8_t alpha) {
    // Platform (wide rect)
    gfx.fillRoundedRect((int16_t)(cx - 50), cy, 100, 10, 3, color, alpha);
    // Left upright rail
    gfx.drawLine((int16_t)(cx - 44), cy,
                 (int16_t)(cx - 40), (int16_t)(cy - 50), color, 3, alpha);
    // Right upright rail
    gfx.drawLine((int16_t)(cx + 44), cy,
                 (int16_t)(cx + 40), (int16_t)(cy - 50), color, 3, alpha);
    // Handle bar connecting top of rails
    gfx.drawLine((int16_t)(cx - 40), (int16_t)(cy - 50),
                 (int16_t)(cx + 40), (int16_t)(cy - 50), color, 2, alpha);
    // Belt lines on platform
    gfx.drawLine((int16_t)(cx - 46), (int16_t)(cy + 4),
                 (int16_t)(cx + 46), (int16_t)(cy + 4), color, 1, (uint8_t)(alpha * 0.5f));
}

/* -- Helper: draw running figure on treadmill ----------------------------- */
static void drawRunner(GfxEngine& gfx, int16_t cx, int16_t cy,
                       float legPhase, float speed, uint16_t color, uint8_t alpha) {
    float bob = fabsf(sinf(legPhase)) * (2.0f + speed);
    int16_t headY = (int16_t)(cy - 32.0f - bob);

    // Head
    gfx.fillCircle(cx, headY, 7, color, alpha);
    // Body
    gfx.drawLine(cx, (int16_t)(headY + 7), cx, (int16_t)(cy - 8),
                 color, 3, alpha);
    // Arms
    float armA = sinf(legPhase) * (12.0f + speed * 4.0f);
    gfx.drawLine(cx, (int16_t)(headY + 12),
                 (int16_t)(cx - 8.0f + armA * 0.4f), (int16_t)(cy - 14),
                 color, 2, alpha);
    gfx.drawLine(cx, (int16_t)(headY + 12),
                 (int16_t)(cx + 8.0f - armA * 0.4f), (int16_t)(cy - 14),
                 color, 2, alpha);
    // Legs
    float legA = sinf(legPhase) * (10.0f + speed * 3.0f);
    gfx.drawLine(cx, (int16_t)(cy - 8),
                 (int16_t)(cx - 6.0f + legA), (int16_t)(cy - 6),
                 color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 8),
                 (int16_t)(cx + 6.0f - legA), (int16_t)(cy - 6),
                 color, 3, alpha);
}

/* -- Helper: draw sweat drops --------------------------------------------- */
static void drawSweat(GfxEngine& gfx, int16_t cx, int16_t cy,
                      float t, uint16_t color) {
    for (int i = 0; i < 3; i++) {
        float dp = fmodf(t * 3.0f + (float)i * 0.33f, 1.0f);
        int16_t sx = (int16_t)(cx + 8 + i * 6);
        int16_t sy = (int16_t)(cy - 28.0f + dp * 20.0f);
        uint8_t op = (uint8_t)((1.0f - dp) * 180);
        gfx.fillCircle(sx, sy, 2, color, op);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_fitness(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, FIT_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "lazy")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "suitUp")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "millIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "warmup", "run")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "sprint")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "cooldown")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIn(ph.name, "fist", "millOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
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
    int16_t millCx = SCX, millCy = CY + 40;

    if (phaseIs(ph.name, "suitUp")) {
        // Pulsing dots
        for (int i = 0; i < 3; i++) {
            float dp = sinf(ph.p * TAU * 2.0f + (float)i * 1.2f);
            uint8_t op = (uint8_t)(clamp(dp, 0.0f, 1.0f) * 180);
            gfx.fillCircle((int16_t)(SCX - 20 + i * 20), CY + 30, 4, colors.eye, op);
        }
    } else if (phaseIs(ph.name, "millIn")) {
        // Treadmill rises from bottom
        float ep = ease::out(ph.p);
        int16_t my = (int16_t)lerp((float)(SCREEN_H + 30), (float)millCy, ep);
        drawTreadmill(gfx, millCx, my, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "warmup")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
        // Slow running
        float legPhase = ph.p * TAU * 4.0f;
        drawRunner(gfx, millCx, millCy, legPhase, 1.0f, colors.eye, 255);

        // HR text (low)
        int hr = 80 + (int)(ph.p * 20.0f);
        char hrBuf[12];
        snprintf(hrBuf, sizeof(hrBuf), "%d", hr);
        gfx.drawText(hrBuf, 46, STATUS_H + 36, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("BPM", 46, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);
    } else if (phaseIs(ph.name, "run")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
        // Faster running
        float legPhase = ph.p * TAU * 8.0f;
        drawRunner(gfx, millCx, millCy, legPhase, 2.0f, colors.eye, 255);

        // HR climbing
        int hr = 100 + (int)(ph.p * 40.0f);
        char hrBuf[12];
        snprintf(hrBuf, sizeof(hrBuf), "%d", hr);
        gfx.drawText(hrBuf, 46, STATUS_H + 36, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("BPM", 46, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);
    } else if (phaseIs(ph.name, "sprint")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
        // Fastest running
        float legPhase = ph.p * TAU * 12.0f;
        drawRunner(gfx, millCx, millCy, legPhase, 3.5f, colors.eye, 255);

        // Sweat drops
        drawSweat(gfx, millCx, millCy, ph.p, colors.eye);

        // HR high
        int hr = 140 + (int)(ph.p * 20.0f);
        char hrBuf[12];
        snprintf(hrBuf, sizeof(hrBuf), "%d", hr);
        gfx.drawText(hrBuf, 46, STATUS_H + 36, TONE_LUT[TONE_RED], 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("BPM", 46, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);

        // Speed lines
        for (int i = 0; i < 3; i++) {
            float lp = fmodf(ph.p * 10.0f + (float)i * 0.33f, 1.0f);
            gfx.drawLine((int16_t)(millCx - 30.0f - lp * 24.0f),
                         (int16_t)(millCy - 20 + i * 10),
                         (int16_t)(millCx - 18.0f - lp * 14.0f),
                         (int16_t)(millCy - 20 + i * 10),
                         colors.eye, 2, (uint8_t)((1.0f - lp) * 160));
        }
    } else if (phaseIs(ph.name, "cooldown")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
        // Slowing down
        float legPhase = ph.p * TAU * 3.0f;
        drawRunner(gfx, millCx, millCy, legPhase, 1.0f, colors.eye, 255);

        // HR dropping
        int hr = 160 - (int)(ph.p * 50.0f);
        char hrBuf[12];
        snprintf(hrBuf, sizeof(hrBuf), "%d", hr);
        gfx.drawText(hrBuf, 46, STATUS_H + 36, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText("BPM", 46, STATUS_H + 52, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 160);
    } else if (phaseIs(ph.name, "fist")) {
        drawTreadmill(gfx, millCx, millCy, colors.eye, 255);
        // Standing figure with fist raised
        int16_t headY = (int16_t)(millCy - 34);
        gfx.fillCircle(millCx, headY, 7, colors.eye);
        gfx.drawLine(millCx, (int16_t)(headY + 7), millCx, (int16_t)(millCy - 8),
                     colors.eye, 3);
        // Fist raised (right arm up)
        gfx.drawLine(millCx, (int16_t)(headY + 12),
                     (int16_t)(millCx - 10), (int16_t)(millCy - 16),
                     colors.eye, 2);
        gfx.drawLine(millCx, (int16_t)(headY + 12),
                     (int16_t)(millCx + 12), (int16_t)(headY - 6),
                     colors.eye, 2);
        // Fist circle
        gfx.fillCircle((int16_t)(millCx + 12), (int16_t)(headY - 8), 4, colors.eye);
        // Legs standing
        gfx.drawLine(millCx, (int16_t)(millCy - 8),
                     (int16_t)(millCx - 6), (int16_t)(millCy - 6),
                     colors.eye, 3);
        gfx.drawLine(millCx, (int16_t)(millCy - 8),
                     (int16_t)(millCx + 6), (int16_t)(millCy - 6),
                     colors.eye, 3);
    } else if (phaseIs(ph.name, "millOut")) {
        // Treadmill exits downward
        float ep = ease::in(ph.p);
        int16_t my = (int16_t)lerp((float)millCy, (float)(SCREEN_H + 40), ep);
        drawTreadmill(gfx, millCx, my, colors.eye, (uint8_t)((1.0f - ep) * 255));
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIs(ph.name, "warmup")) {
        drawActLabel(gfx, "WARM UP", colors.eye);
    } else if (phaseIs(ph.name, "run")) {
        drawActLabel(gfx, "RUNNING", colors.eye);
    } else if (phaseIs(ph.name, "sprint")) {
        drawActLabel(gfx, "SPRINT!", colors.eye);
    } else if (phaseIs(ph.name, "cooldown")) {
        drawActLabel(gfx, "COOL DOWN", colors.eye);
    } else if (phaseIs(ph.name, "fist")) {
        drawActLabel(gfx, "CRUSHED IT!", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "WORKOUT DONE", colors.eye);
    }
}

const VariantDef FITNESS_VARIANTS[] = {
    {"fitness-run", "Fitness Run", 14000, TONE_NONE, render_fitness},
};

extern const CategoryDef CAT_FITNESS = {
    "fitness", "Fitness", ContentKind::SCENE, TONE_RED,
    FITNESS_VARIANTS, sizeof(FITNESS_VARIANTS) / sizeof(FITNESS_VARIANTS[0]), false
};
