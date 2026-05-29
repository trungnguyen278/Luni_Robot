#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry TIMER_PHASES[] = {
    {"think",    0.06f},   // Luni thinking, centered
    {"assemble", 0.10f},   // 4 digits fly in from corners to form "05:00"
    {"shrink",   0.05f},   // Eyes shrink to top-right corner
    {"count",    0.36f},   // Countdown: digits change, arc progress
    {"ding",     0.10f},   // DING! shake + sparkle burst
    {"scatter",  0.10f},   // Digits fly back to corners
    {"grow",     0.07f},   // Eyes grow back to center
    {"done",     0.16f},   // Excited Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(TIMER_PHASES) / sizeof(TIMER_PHASES[0]);

/* -- Helper: digit position corners -------------------------------------- */
struct CornerPos { float x, y; };
static const CornerPos DIGIT_CORNERS[4] = {
    { 20.0f,  30.0f},   // top-left
    {300.0f,  30.0f},   // top-right
    { 20.0f, 210.0f},   // bottom-left
    {300.0f, 210.0f},   // bottom-right
};

/* Assembled positions: "MM:SS" centered on screen */
static const float DIGIT_X[4] = {
    (float)SCX - 68.0f,   // tens of minutes
    (float)SCX - 32.0f,   // ones of minutes
    (float)SCX + 32.0f,   // tens of seconds
    (float)SCX + 68.0f,   // ones of seconds
};
static constexpr float DIGIT_Y = (float)CY + 8.0f;

/* -- Helper: draw sparkle burst ------------------------------------------ */
static void drawSparkleBurst(GfxEngine& gfx, int16_t cx, int16_t cy,
                             float p, uint16_t color) {
    int sparkCount = 8;
    for (int i = 0; i < sparkCount; i++) {
        float a = (float)i / (float)sparkCount * TAU + p * 0.5f;
        float dist = 20.0f + p * 50.0f;
        uint8_t alpha = (uint8_t)((1.0f - p) * 255);
        int16_t sx = (int16_t)(cx + cosf(a) * dist);
        int16_t sy = (int16_t)(cy + sinf(a) * dist);
        int16_t sz = (int16_t)(3.0f - p * 2.0f);
        if (sz < 1) sz = 1;
        gfx.fillCircle(sx, sy, sz, color, alpha);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_timer_countdown(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, TIMER_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "thinking";

    if (phaseIs(ph.name, "think")) {
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "assemble")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIs(ph.name, "count")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "ding")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "scatter")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "excited";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "excited";
    }

    /* --- Timer props -------------------------------------------------- */
    if (phaseIs(ph.name, "assemble")) {
        // Digits fly from corners toward center positions
        float ep = ease::out(ph.p);
        const char* startDigits[4] = {"0", "5", "0", "0"};
        for (int i = 0; i < 4; i++) {
            float dx = lerp(DIGIT_CORNERS[i].x, DIGIT_X[i], ep);
            float dy = lerp(DIGIT_CORNERS[i].y, DIGIT_Y, ep);
            uint8_t alpha = (uint8_t)(ep * 255);
            gfx.drawText(startDigits[i], (int16_t)dx, (int16_t)dy,
                         colors.eye, 5, GfxEngine::TextAlign::CENTER, alpha);
        }
        // Colon
        uint8_t colonAlpha = (uint8_t)(ep * 255);
        gfx.drawText(":", SCX, (int16_t)DIGIT_Y - 4, colors.eye, 4,
                     GfxEngine::TextAlign::CENTER, colonAlpha);
    } else if (phaseIs(ph.name, "shrink")) {
        // Display "05:00" static
        gfx.drawText("0", (int16_t)DIGIT_X[0], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText("5", (int16_t)DIGIT_X[1], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText(":", SCX, (int16_t)DIGIT_Y - 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
        gfx.drawText("0", (int16_t)DIGIT_X[2], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText("0", (int16_t)DIGIT_X[3], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
    } else if (phaseIs(ph.name, "count")) {
        // Countdown from 5:00 to 0:00
        int totalSec = (int)(300.0f * (1.0f - ph.p));
        if (totalSec < 0) totalSec = 0;
        int mm = totalSec / 60;
        int ss = totalSec % 60;

        char d0[2], d1[2], d2[2], d3[2];
        snprintf(d0, 2, "%d", mm / 10);
        snprintf(d1, 2, "%d", mm % 10);
        snprintf(d2, 2, "%d", ss / 10);
        snprintf(d3, 2, "%d", ss % 10);

        gfx.drawText(d0, (int16_t)DIGIT_X[0], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText(d1, (int16_t)DIGIT_X[1], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        // Blinking colon
        uint8_t colonOp = ((int)(ph.p * 10.0f) % 2 == 0) ? 255 : 80;
        gfx.drawText(":", SCX, (int16_t)DIGIT_Y - 4, colors.eye, 4, GfxEngine::TextAlign::CENTER, colonOp);
        gfx.drawText(d2, (int16_t)DIGIT_X[2], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText(d3, (int16_t)DIGIT_X[3], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);

        // Progress arc around the digits area
        float arcR = 80.0f;
        gfx.drawArc(SCX, (int16_t)DIGIT_Y, (int16_t)arcR,
                     0, TAU, colors.eye, 3, 40);
        float endAngle = -PI / 2.0f + ph.p * TAU;
        gfx.drawArc(SCX, (int16_t)DIGIT_Y, (int16_t)arcR,
                     -PI / 2.0f, endAngle, colors.eye, 4);
    } else if (phaseIs(ph.name, "ding")) {
        // "00:00" with shake
        float shake = sinf(ph.p * TAU * 10.0f) * 6.0f * (1.0f - ph.p);
        gfx.pushTransform();
        gfx.translate(shake, 0);

        gfx.drawText("0", (int16_t)DIGIT_X[0], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText("0", (int16_t)DIGIT_X[1], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText(":", SCX, (int16_t)DIGIT_Y - 4, colors.eye, 4, GfxEngine::TextAlign::CENTER);
        gfx.drawText("0", (int16_t)DIGIT_X[2], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);
        gfx.drawText("0", (int16_t)DIGIT_X[3], (int16_t)DIGIT_Y, colors.eye, 5, GfxEngine::TextAlign::CENTER);

        gfx.popTransform();

        // "DING!" text
        uint8_t dingAlpha = (uint8_t)(sinf(ph.p * TAU * 3.0f) * 80.0f + 175.0f);
        gfx.drawText("DING!", SCX, STATUS_H + 24, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, dingAlpha);

        // Sparkle burst
        drawSparkleBurst(gfx, SCX, (int16_t)DIGIT_Y, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "scatter")) {
        // Digits fly back to corners
        float ep = ease::in(ph.p);
        const char* endDigits[4] = {"0", "0", "0", "0"};
        for (int i = 0; i < 4; i++) {
            float dx = lerp(DIGIT_X[i], DIGIT_CORNERS[i].x, ep);
            float dy = lerp(DIGIT_Y, DIGIT_CORNERS[i].y, ep);
            uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
            gfx.drawText(endDigits[i], (int16_t)dx, (int16_t)dy,
                         colors.eye, 5, GfxEngine::TextAlign::CENTER, alpha);
        }
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   colors.eye, colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "count", "ding")) {
        drawActLabel(gfx, "TIMER", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "DONE!", colors.eye);
    }
}

const VariantDef TIMER_VARIANTS[] = {
    {"timer-countdown", "Countdown", 14000, TONE_NONE, render_timer_countdown},
};

extern const CategoryDef CAT_TIMER = {
    "timer", "Timer", ContentKind::SCENE, TONE_ORANGE,
    TIMER_VARIANTS, sizeof(TIMER_VARIANTS) / sizeof(TIMER_VARIANTS[0]), false
};
