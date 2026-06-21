#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry STOPWATCH_PHASES[] = {
    {"ready",    0.04f},   // Eager Luni, centered
    {"watchIn",  0.06f},   // Stopwatch drops in from bottom
    {"shrink",   0.04f},   // Eyes shrink to top-right corner
    {"start",    0.05f},   // START button press, begin ticking
    {"run1",     0.18f},   // Sweep hand rotates, time counts up
    {"lap1",     0.04f},   // LAP 1 marker flashes
    {"run2",     0.18f},   // More ticking
    {"lap2",     0.04f},   // LAP 2 marker flashes
    {"run3",     0.14f},   // Final stretch
    {"stop",     0.05f},   // STOP — hand halts, flash
    {"result",   0.06f},   // Final time display
    {"watchOut", 0.04f},   // Watch exits down
    {"grow",     0.04f},   // Eyes grow back to center
    {"done",     0.04f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(STOPWATCH_PHASES) / sizeof(STOPWATCH_PHASES[0]);

/* ── Helper: draw stopwatch body ─────────────────────────────────── */
static void drawWatchBody(GfxEngine& gfx, int16_t cx, int16_t cy,
                          uint16_t color, uint8_t alpha) {
    int16_t mainR = 48;

    // Main circle
    gfx.strokeCircle(cx, cy, mainR, color, 3);
    gfx.strokeCircle(cx, cy, (int16_t)(mainR - 4), color, 1);

    // Tick marks (12 positions)
    for (int i = 0; i < 12; i++) {
        float angle = (float)i * (TAU / 12.0f) - PI / 2.0f;
        float innerR = (i % 3 == 0) ? (float)mainR - 12.0f : (float)mainR - 8.0f;
        int16_t sw = (i % 3 == 0) ? 2 : 1;
        int16_t x0 = (int16_t)(cx + cosf(angle) * innerR);
        int16_t y0 = (int16_t)(cy + sinf(angle) * innerR);
        int16_t x1 = (int16_t)(cx + cosf(angle) * ((float)mainR - 5));
        int16_t y1 = (int16_t)(cy + sinf(angle) * ((float)mainR - 5));
        gfx.drawLine(x0, y0, x1, y1, color, sw, alpha);
    }

    // Crown button (top)
    gfx.fillRoundedRect((int16_t)(cx - 4), (int16_t)(cy - mainR - 12),
                        8, 12, 2, color, alpha);
    // Side button (right)
    gfx.fillRoundedRect((int16_t)(cx + mainR + 2), (int16_t)(cy - 6),
                        8, 6, 1, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx + mainR + 2), (int16_t)(cy + 2),
                        8, 6, 1, color, alpha);
}

/* ── Helper: draw sweep hand ─────────────────────────────────────── */
static void drawSweepHand(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float angle, uint16_t color, uint8_t alpha) {
    float handLen = 38.0f;
    int16_t x1 = (int16_t)(cx + cosf(angle) * handLen);
    int16_t y1 = (int16_t)(cy + sinf(angle) * handLen);
    gfx.drawLine(cx, cy, x1, y1, color, 2, alpha);
    // Center dot
    gfx.fillCircle(cx, cy, 3, color, alpha);
    // Tip dot
    gfx.fillCircle(x1, y1, 2, color, alpha);
}

/* ── Helper: draw time text ──────────────────────────────────────── */
static void drawTimeText(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float seconds, uint16_t color, uint8_t alpha,
                         uint8_t scale) {
    int totalCs = (int)(seconds * 100.0f);
    int sec = totalCs / 100;
    int cs  = totalCs % 100;
    int mm  = sec / 60;
    sec = sec % 60;

    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d.%02d", mm, sec, cs);
    gfx.drawText(buf, cx, cy, color, scale, GfxEngine::TextAlign::CENTER, alpha);
}

/* ── Helper: draw LAP badge ──────────────────────────────────────── */
static void drawLapBadge(GfxEngine& gfx, int16_t cx, int16_t cy,
                         int lapNum, float lapTime, float fadeP,
                         uint16_t color) {
    uint8_t alpha = (uint8_t)(ease::out(fadeP) * 240);

    // Badge background
    gfx.fillRoundedRect((int16_t)(cx - 46), (int16_t)(cy - 10),
                        92, 20, 4, color, (uint8_t)(alpha / 4));
    gfx.strokeRoundedRect((int16_t)(cx - 46), (int16_t)(cy - 10),
                          92, 20, 4, color, 1);

    // Lap label
    char buf[16];
    int sec = (int)lapTime;
    int cs  = (int)((lapTime - (float)sec) * 100.0f);
    snprintf(buf, sizeof(buf), "LAP %d  %d.%02d", lapNum, sec, cs);
    gfx.drawText(buf, cx, (int16_t)(cy + 4), color, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_stopwatch(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, STOPWATCH_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "ready")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "watchIn")) {
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "excited";
    } else if (phaseIs(ph.name, "start")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIn(ph.name, "run1", "run2", "run3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIn(ph.name, "lap1", "lap2")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "stop")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "result")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "watchOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Stopwatch prop ------------------------------------------- */
    int16_t wCx = SCX, wCy = CY + 8;

    /* Compute cumulative elapsed "stopwatch seconds" across run phases.
       Total run phases = run1(0.18) + lap1(0.04) + run2(0.18) + lap2(0.04) + run3(0.14) = 0.58
       We simulate 45 seconds of stopwatch time across those phases. */
    float simTime = 0.0f;  // simulated stopwatch seconds
    float sweepAngle = -PI / 2.0f;
    bool ticking = false;

    if (phaseIs(ph.name, "run1")) {
        simTime = ph.p * 15.0f;
        ticking = true;
    } else if (phaseIs(ph.name, "lap1")) {
        simTime = 15.0f;
    } else if (phaseIs(ph.name, "run2")) {
        simTime = 15.0f + ph.p * 15.0f;
        ticking = true;
    } else if (phaseIs(ph.name, "lap2")) {
        simTime = 30.0f;
    } else if (phaseIs(ph.name, "run3")) {
        simTime = 30.0f + ph.p * 15.0f;
        ticking = true;
    } else if (phaseIn(ph.name, "stop", "result", "watchOut")) {
        simTime = 45.0f;
    }

    sweepAngle = -PI / 2.0f + (simTime / 60.0f) * TAU;

    if (phaseIs(ph.name, "watchIn")) {
        float ep = ease::out(ph.p);
        int16_t fromY = SCREEN_H + 60;
        wCy = (int16_t)lerp((float)fromY, (float)(CY + 8), ep);
        drawWatchBody(gfx, wCx, wCy, colors.eye, (uint8_t)(ep * 255));
        drawSweepHand(gfx, wCx, wCy, -PI / 2.0f, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, -PI / 2.0f, colors.eye, 255);
    } else if (phaseIs(ph.name, "start")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, -PI / 2.0f, colors.eye, 255);
        // "START" flash text
        uint8_t startOp = (uint8_t)(sinf(ph.p * PI) * 255);
        gfx.drawText("GO!", wCx, (int16_t)(wCy + 18), colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, startOp);
    } else if (phaseIn(ph.name, "run1", "run2", "run3")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, 255);
        // Time display below watch
        drawTimeText(gfx, wCx, (int16_t)(wCy + 62), simTime,
                     colors.eye, 200, 2);
    } else if (phaseIs(ph.name, "lap1")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, 255);
        drawTimeText(gfx, wCx, (int16_t)(wCy + 62), simTime,
                     colors.eye, 200, 2);
        // LAP 1 badge
        drawLapBadge(gfx, wCx, STATUS_H + 28, 1, 15.0f, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "lap2")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, 255);
        drawTimeText(gfx, wCx, (int16_t)(wCy + 62), simTime,
                     colors.eye, 200, 2);
        // LAP 2 badge
        drawLapBadge(gfx, wCx, STATUS_H + 28, 2, 15.0f, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "stop")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, 255);
        drawTimeText(gfx, wCx, (int16_t)(wCy + 62), simTime,
                     colors.eye, 255, 2);
        // "STOP" flash
        uint8_t stopOp = (uint8_t)(sinf(ph.p * PI) * 255);
        gfx.drawText("STOP", wCx, STATUS_H + 28, TONE_LUT[TONE_RED], 2,
                     GfxEngine::TextAlign::CENTER, stopOp);
    } else if (phaseIs(ph.name, "result")) {
        drawWatchBody(gfx, wCx, wCy, colors.eye, 255);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, 255);

        // Large final time
        float fadeIn = ease::out(ph.p);
        drawTimeText(gfx, wCx, (int16_t)(wCy + 62), simTime,
                     colors.eye, (uint8_t)(fadeIn * 255), 3);

        // Lap list below
        char lap1Buf[20], lap2Buf[20];
        snprintf(lap1Buf, sizeof(lap1Buf), "L1: 0:15.00");
        snprintf(lap2Buf, sizeof(lap2Buf), "L2: 0:15.00");
        uint8_t listOp = (uint8_t)(fadeIn * 160);
        gfx.drawText(lap1Buf, wCx, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, listOp);
        gfx.drawText(lap2Buf, wCx, STATUS_H + 40, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, listOp);
    } else if (phaseIs(ph.name, "watchOut")) {
        float ep = ease::in(ph.p);
        wCy = (int16_t)lerp((float)(CY + 8), (float)(SCREEN_H + 70), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawWatchBody(gfx, wCx, wCy, colors.eye, alpha);
        drawSweepHand(gfx, wCx, wCy, sweepAngle, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (ticking) {
        drawActLabel(gfx, "TIMING", colors.eye);
    } else if (phaseIn(ph.name, "stop", "result")) {
        drawActLabel(gfx, "FINISHED", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "PERSONAL BEST", colors.eye);
    }
}

const VariantDef STOPWATCH_VARIANTS[] = {
    {"stopwatch-lap", "Lap timer", 14000, TONE_NONE, render_stopwatch},
};

extern const CategoryDef CAT_STOPWATCH = {
    "stopwatch", "Stopwatch", ContentKind::SCENE, TONE_ORANGE,
    STOPWATCH_VARIANTS, sizeof(STOPWATCH_VARIANTS) / sizeof(STOPWATCH_VARIANTS[0]),
    false
};
