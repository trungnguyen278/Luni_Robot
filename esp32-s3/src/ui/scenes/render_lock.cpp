#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry LOCK_PHASES[] = {
    {"ask",       0.06f},   // Luni curious, centered
    {"lockIn",    0.07f},   // Padlock drops in from top
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"scan1",     0.10f},   // Fingerprint scan (wave arcs)
    {"fail",      0.08f},   // FAIL — red shake, X mark
    {"retry",     0.04f},   // Brief pause, try again
    {"scan2",     0.12f},   // Second scan, sweep line
    {"success",   0.07f},   // SUCCESS — checkmark
    {"shackleUp", 0.08f},   // Shackle pops up (unlock anim)
    {"open",      0.10f},   // Lock shown in unlocked state
    {"lockOut",   0.07f},   // Lock exits down
    {"grow",      0.06f},   // Eyes grow back to center
    {"done",      0.10f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(LOCK_PHASES) / sizeof(LOCK_PHASES[0]);

/* ── Helper: draw padlock body ───────────────────────────────────── */
static void drawLockBody(GfxEngine& gfx, int16_t cx, int16_t cy,
                         uint16_t color, uint8_t alpha) {
    // Body rectangle
    gfx.fillRoundedRect((int16_t)(cx - 24), cy, 48, 36, 4, color, alpha);
    // Keyhole
    gfx.fillCircle(cx, (int16_t)(cy + 12), 5, COLOR_BG, alpha);
    gfx.fillRect((int16_t)(cx - 2), (int16_t)(cy + 14), 4, 10, COLOR_BG, alpha);
}

/* ── Helper: draw shackle (locked or unlocked) ───────────────────── */
static void drawShackle(GfxEngine& gfx, int16_t cx, int16_t cy,
                        float openP, uint16_t color, uint8_t alpha) {
    // Shackle as an arc + two vertical lines
    int16_t shackleTop = (int16_t)(cy - 16.0f - openP * 14.0f);
    int16_t shackleR = 16;

    // Arc (top half-circle)
    gfx.drawArc(cx, shackleTop, shackleR, PI, TAU, color, 4, alpha);

    // Left leg (always connected)
    gfx.drawLine((int16_t)(cx - shackleR), shackleTop,
                 (int16_t)(cx - shackleR), cy, color, 4, alpha);

    // Right leg (lifts up when unlocking)
    int16_t rightLegBot = (int16_t)(cy - openP * 18.0f);
    gfx.drawLine((int16_t)(cx + shackleR), shackleTop,
                 (int16_t)(cx + shackleR), rightLegBot, color, 4, alpha);
}

/* ── Helper: draw fingerprint scan arcs ──────────────────────────── */
static void drawScanWaves(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float p, uint16_t color) {
    // Concentric arcs expanding outward (fingerprint feel)
    for (int i = 0; i < 4; i++) {
        float waveP = fmodf(p * 2.0f + (float)i * 0.25f, 1.0f);
        int16_t r = (int16_t)(8.0f + waveP * 28.0f);
        uint8_t alpha = (uint8_t)((1.0f - waveP) * 180);
        gfx.drawArc(cx, cy, r, -PI * 0.6f, PI * 0.6f, color, 2, alpha);
    }
}

/* ── Helper: draw X mark ─────────────────────────────────────────── */
static void drawXMark(GfxEngine& gfx, int16_t cx, int16_t cy,
                      int16_t sz, uint16_t color, uint8_t alpha) {
    gfx.drawLine((int16_t)(cx - sz), (int16_t)(cy - sz),
                 (int16_t)(cx + sz), (int16_t)(cy + sz), color, 3, alpha);
    gfx.drawLine((int16_t)(cx + sz), (int16_t)(cy - sz),
                 (int16_t)(cx - sz), (int16_t)(cy + sz), color, 3, alpha);
}

/* ── Helper: draw checkmark ──────────────────────────────────────── */
static void drawCheckmark(GfxEngine& gfx, int16_t cx, int16_t cy,
                          int16_t sz, uint16_t color, uint8_t alpha) {
    gfx.drawLine((int16_t)(cx - sz), cy,
                 (int16_t)(cx - sz / 3), (int16_t)(cy + sz), color, 4, alpha);
    gfx.drawLine((int16_t)(cx - sz / 3), (int16_t)(cy + sz),
                 (int16_t)(cx + sz), (int16_t)(cy - sz), color, 4, alpha);
}

/* ── Helper: draw scan line (horizontal sweep) ───────────────────── */
static void drawScanLine(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float p, uint16_t color) {
    int16_t lineY = (int16_t)(cy - 14.0f + p * 28.0f);
    gfx.drawLine((int16_t)(cx - 22), lineY,
                 (int16_t)(cx + 22), lineY, color, 2, 178);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_lock(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, LOCK_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "ask")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "lockIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIs(ph.name, "scan1")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "fail")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "retry")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "scan2")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "success")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIn(ph.name, "shackleUp", "open")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "lockOut")) {
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

    /* --- Padlock prop --------------------------------------------- */
    int16_t lockCx = SCX, lockCy = CY + 8;
    float lockShake = 0.0f;
    float shackleOpen = 0.0f;

    if (phaseIs(ph.name, "lockIn")) {
        float ep = ease::out(ph.p);
        int16_t fromY = STATUS_H - 70;
        lockCy = (int16_t)lerp((float)fromY, (float)(CY + 8), ep);
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, (uint8_t)(ep * 255));
        drawLockBody(gfx, lockCx, lockCy, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "scan1")) {
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
        drawScanWaves(gfx, lockCx, (int16_t)(lockCy + 12), ph.p, colors.eye);
        drawScanLine(gfx, lockCx, lockCy, fmodf(ph.p * 3.0f, 1.0f), colors.eye);
    } else if (phaseIs(ph.name, "fail")) {
        // Shake the lock horizontally
        lockShake = sinf(ph.p * TAU * 6.0f) * 10.0f * (1.0f - ph.p);
        int16_t shakeCx = (int16_t)((float)lockCx + lockShake);
        drawShackle(gfx, shakeCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, shakeCx, lockCy, colors.eye, 255);
        // X mark fades in
        uint8_t xAlpha = (uint8_t)(ease::out(ph.p) * 255);
        drawXMark(gfx, lockCx, (int16_t)(lockCy - 40), 12,
                  TONE_LUT[TONE_RED], xAlpha);
        gfx.drawText("DENIED", SCX, STATUS_H + 28, TONE_LUT[TONE_RED], 1,
                     GfxEngine::TextAlign::CENTER, xAlpha);
    } else if (phaseIs(ph.name, "retry")) {
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
        // Blink dots to indicate retry
        uint8_t dotOp = (uint8_t)(sinf(ph.p * TAU * 2.0f) * 127 + 128);
        for (int i = 0; i < 3; i++) {
            gfx.fillCircle((int16_t)(lockCx - 10 + i * 10),
                           (int16_t)(lockCy + 48), 3, colors.eye, dotOp);
        }
    } else if (phaseIs(ph.name, "scan2")) {
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
        drawScanWaves(gfx, lockCx, (int16_t)(lockCy + 12), ph.p, colors.eye);
        drawScanLine(gfx, lockCx, lockCy, fmodf(ph.p * 4.0f, 1.0f), colors.eye);
        // Progress indicator
        gfx.fillRect((int16_t)(lockCx - 30), (int16_t)(lockCy + 48),
                     (int16_t)(60.0f * ph.p), 3, colors.eye, 178);
    } else if (phaseIs(ph.name, "success")) {
        drawShackle(gfx, lockCx, lockCy, 0.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
        // Checkmark appears
        float fadeIn = ease::out(ph.p);
        uint8_t checkAlpha = (uint8_t)(fadeIn * 255);
        drawCheckmark(gfx, lockCx, (int16_t)(lockCy - 40), 12,
                      TONE_LUT[TONE_GREEN], checkAlpha);
        gfx.drawText("GRANTED", SCX, STATUS_H + 28, TONE_LUT[TONE_GREEN], 1,
                     GfxEngine::TextAlign::CENTER, checkAlpha);
    } else if (phaseIs(ph.name, "shackleUp")) {
        shackleOpen = ease::out(ph.p);
        drawShackle(gfx, lockCx, lockCy, shackleOpen, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "open")) {
        drawShackle(gfx, lockCx, lockCy, 1.0f, colors.eye, 255);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, 255);
        // Sparkle around unlocked lock
        for (int i = 0; i < 6; i++) {
            float angle = (float)i * (TAU / 6.0f) + ph.p * TAU * 0.5f;
            int16_t sx = (int16_t)(lockCx + cosf(angle) * 42.0f);
            int16_t sy = (int16_t)(lockCy + sinf(angle) * 30.0f);
            uint8_t sparkOp = (uint8_t)(fabsf(sinf(ph.p * TAU * 2.0f +
                              (float)i)) * 200);
            gfx.fillCircle(sx, sy, 2, colors.eye, sparkOp);
        }
    } else if (phaseIs(ph.name, "lockOut")) {
        float ep = ease::in(ph.p);
        lockCy = (int16_t)lerp((float)(CY + 8), (float)(SCREEN_H + 60), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawShackle(gfx, lockCx, lockCy, 1.0f, colors.eye, alpha);
        drawLockBody(gfx, lockCx, lockCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_GREEN], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "scan1", "scan2")) {
        drawActLabel(gfx, "SCANNING", colors.eye);
    } else if (phaseIn(ph.name, "shackleUp", "open")) {
        drawActLabel(gfx, "UNLOCKED", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "SECURE", colors.eye);
    }
}

const VariantDef LOCK_VARIANTS[] = {
    {"lock-main", "Main", 14000, TONE_NONE, render_lock},
};

extern const CategoryDef CAT_LOCK = {
    "lock", "Lock", ContentKind::SCENE, TONE_GREEN,
    LOCK_VARIANTS, sizeof(LOCK_VARIANTS) / sizeof(LOCK_VARIANTS[0]), false
};
