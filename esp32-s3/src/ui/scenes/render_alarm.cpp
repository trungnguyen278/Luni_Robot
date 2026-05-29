#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry ALARM_PHASES[] = {
    {"idle",    0.06f},   // Luni sleepy, centered
    {"propIn",  0.07f},   // Bell drops from top
    {"shrink",  0.06f},   // Eyes shrink to top-right corner
    {"ring1",   0.18f},   // Bell shakes left-right
    {"ring2",   0.18f},   // Bell shakes harder, sound wave arcs
    {"ring3",   0.12f},   // "RING!" text flashes, climax
    {"propOut", 0.07f},   // Bell exits upward
    {"grow",    0.06f},   // Eyes grow back to center
    {"done",    0.10f},   // Happy/awake Luni
    {"pad",     0.10f},   // Hold final pose
};
static constexpr uint8_t PHASE_COUNT = sizeof(ALARM_PHASES) / sizeof(ALARM_PHASES[0]);

/* ── Helper: draw bell shape ─────────────────────────────────────── */
static void drawBell(GfxEngine& gfx, int16_t cx, int16_t cy, float shake,
                     uint16_t color, uint8_t alpha) {
    int16_t sx = (int16_t)shake;

    // Dome (circle top half)
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy - 18), 22, color, alpha);

    // Body (trapezoid approximation: wider rect at bottom)
    gfx.fillRoundedRect((int16_t)(cx - 20 + sx), (int16_t)(cy - 8),
                        40, 28, 4, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - 28 + sx), (int16_t)(cy + 14),
                        56, 10, 5, color, alpha);

    // Clapper (small circle at bottom)
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy + 28), 5, color, alpha);

    // Handle arc at top
    gfx.drawArc((int16_t)(cx + sx), (int16_t)(cy - 38), 8,
                PI, TAU, color, 3, alpha);
}

/* ── Helper: draw sound wave arc ─────────────────────────────────── */
static void drawSoundWave(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float radius, float p, uint16_t color) {
    uint8_t alpha = (uint8_t)((1.0f - p) * 200);
    float r = radius + p * 30.0f;
    // Left wave
    gfx.drawArc((int16_t)(cx - 36), cy, (int16_t)r,
                -0.6f, 0.6f, color, 2, alpha);
    // Right wave
    gfx.drawArc((int16_t)(cx + 36), cy, (int16_t)r,
                PI - 0.6f, PI + 0.6f, color, 2, alpha);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_alarm(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, ALARM_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "propIn")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx   = lerp((float)SCX, 284.0f, ep);
        eyeCy   = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo  = "surprised";
    } else if (phaseIn(ph.name, "ring1", "ring2", "ring3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "propOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx   = lerp(284.0f, (float)SCX, ep);
        eyeCy   = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo  = "happy";
    } else if (phaseIn(ph.name, "done", "pad")) {
        eyeEmo = "happy";
    }

    /* --- Bell prop ------------------------------------------------ */
    int16_t bellCx = SCX, bellCy = CY + 4;

    if (phaseIs(ph.name, "propIn")) {
        // Drop from top of screen
        float ep = ease::out(ph.p);
        int16_t fromY = STATUS_H - 60;
        bellCy = (int16_t)lerp((float)fromY, (float)(CY + 4), ep);
        drawBell(gfx, bellCx, bellCy, 0.0f, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawBell(gfx, bellCx, bellCy, 0.0f, colors.eye, 255);
    } else if (phaseIs(ph.name, "ring1")) {
        // Gentle shake
        float shake = sinf(ph.p * TAU * 6.0f) * (4.0f + ph.p * 6.0f);
        drawBell(gfx, bellCx, bellCy, shake, colors.eye, 255);
    } else if (phaseIs(ph.name, "ring2")) {
        // Harder shake + sound waves
        float shake = sinf(ph.p * TAU * 8.0f) * 12.0f;
        drawBell(gfx, bellCx, bellCy, shake, colors.eye, 255);

        // Sound wave arcs (3 layers, staggered)
        for (int i = 0; i < 3; i++) {
            float waveP = fmodf(ph.p * 2.0f + (float)i * 0.33f, 1.0f);
            drawSoundWave(gfx, bellCx, bellCy, 10.0f, waveP, colors.eye);
        }
    } else if (phaseIs(ph.name, "ring3")) {
        // Max shake + "RING!" text
        float shake = sinf(ph.p * TAU * 10.0f) * 14.0f;
        drawBell(gfx, bellCx, bellCy, shake, colors.eye, 255);

        // Sound waves
        for (int i = 0; i < 3; i++) {
            float waveP = fmodf(ph.p * 3.0f + (float)i * 0.33f, 1.0f);
            drawSoundWave(gfx, bellCx, bellCy, 12.0f, waveP, colors.eye);
        }

        // "RING!" text with flash
        uint8_t textAlpha = (uint8_t)(sinf(ph.p * TAU * 3.0f) * 80.0f + 175.0f);
        gfx.drawText("RING!", SCX, STATUS_H + 24, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, textAlpha);
    } else if (phaseIs(ph.name, "propOut")) {
        // Bell exits upward
        float ep = ease::in(ph.p);
        int16_t toY = STATUS_H - 70;
        bellCy = (int16_t)lerp((float)(CY + 4), (float)toY, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawBell(gfx, bellCx, bellCy, 0.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_RED], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "ring1", "ring2", "ring3")) {
        drawActLabel(gfx, "ALARM", colors.eye);
    } else if (phaseIn(ph.name, "done", "pad")) {
        drawActLabel(gfx, "AWAKE", colors.eye);
    }
}

const VariantDef ALARM_VARIANTS[] = {
    {"alarm-main", "Alarm", 12000, TONE_NONE, render_alarm},
};

extern const CategoryDef CAT_ALARM = {
    "alarm", "Alarm", ContentKind::SCENE, TONE_RED,
    ALARM_VARIANTS, sizeof(ALARM_VARIANTS) / sizeof(ALARM_VARIANTS[0]), false
};
