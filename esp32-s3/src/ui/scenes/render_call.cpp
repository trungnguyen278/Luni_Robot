#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ======================================================================== */
/*  Shared helpers                                                          */
/* ======================================================================== */

/* -- Helper: draw phone handset ------------------------------------------ */
static void drawPhone(GfxEngine& gfx, int16_t cx, int16_t cy,
                      uint16_t color, uint8_t alpha) {
    // Earpiece
    gfx.fillRoundedRect((int16_t)(cx - 18), (int16_t)(cy - 24), 16, 18, 4, color, alpha);
    // Mouthpiece
    gfx.fillRoundedRect((int16_t)(cx + 4), (int16_t)(cy + 6), 16, 16, 4, color, alpha);
    // Handle connecting them
    gfx.drawLine((int16_t)(cx - 6), (int16_t)(cy - 6),
                 (int16_t)(cx + 8), (int16_t)(cy + 10), color, 6);
}

/* -- Helper: draw vibration arcs ----------------------------------------- */
static void drawVibrationArcs(GfxEngine& gfx, int16_t cx, int16_t cy,
                              float t, uint16_t color) {
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.0f + (float)i * 0.33f, 1.0f);
        int16_t r = (int16_t)(20.0f + p * 24.0f);
        uint8_t op = (uint8_t)((1.0f - p) * 180);
        gfx.drawArc((int16_t)(cx + 18), cy, r,
                    -0.6f, 0.6f, color, 2, op);
    }
}

/* -- Helper: draw EQ/voice bars ------------------------------------------ */
static void drawVoiceBars(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float t, uint16_t color) {
    for (int i = 0; i < 7; i++) {
        float phase = t * TAU * 3.0f + (float)i * 0.9f;
        int16_t h = (int16_t)(4.0f + fabsf(sinf(phase)) * 20.0f);
        int16_t x = (int16_t)(cx - 42 + i * 14);
        gfx.fillRoundedRect((int16_t)(x - 3), (int16_t)(cy + 30 - h),
                            6, h, 2, color);
    }
}

/* -- Helper: draw ring pulse dots ---------------------------------------- */
static void drawRingDots(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float t, uint16_t color) {
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 2.0f + (float)i * 0.33f, 1.0f);
        uint8_t op = (uint8_t)(sinf(p * PI) * 220);
        int16_t r = (int16_t)(18.0f + p * 16.0f);
        gfx.drawArc((int16_t)(cx + 20), cy, r,
                    -0.5f, 0.5f, color, 2, op);
    }
}

/* ======================================================================== */
/*  Variant 1: call-incoming  (14s, TONE_GREEN)                             */
/* ======================================================================== */

static const PhaseEntry INCOMING_PHASES[] = {
    {"idle",     0.06f},   // Steady Luni
    {"phoneIn",  0.07f},   // Phone slides in from left
    {"ring",     0.12f},   // Ring vibrates with arcs
    {"shrink",   0.05f},   // Eyes shrink to corner
    {"ringing",  0.20f},   // Ringing continues
    {"answer",   0.08f},   // Phone expands (answered)
    {"talk",     0.14f},   // EQ bars animate
    {"hangup",   0.06f},   // Phone shrinks
    {"phoneOut", 0.06f},   // Phone exits left
    {"grow",     0.07f},   // Eyes grow back
    {"done",     0.09f},   // Satisfied Luni
};
static constexpr uint8_t INC_PHASE_COUNT = sizeof(INCOMING_PHASES) / sizeof(INCOMING_PHASES[0]);

static void render_call_incoming(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, INCOMING_PHASES, INC_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIn(ph.name, "phoneIn", "ring")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "surprised";
    } else if (phaseIs(ph.name, "ringing")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "answer")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "talk")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIn(ph.name, "hangup", "phoneOut")) {
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

    /* --- Prop: phone -------------------------------------------------- */
    int16_t phoneCx = SCX - 40, phoneCy = CY + 4;

    if (phaseIs(ph.name, "phoneIn")) {
        // Slide in from left
        float ep = ease::out(ph.p);
        phoneCx = (int16_t)lerp(-50.0f, (float)(SCX - 40), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "ring")) {
        // Phone vibrates with arcs
        float wob = sinf(ph.p * TAU * 6.0f) * 5.0f;
        drawPhone(gfx, (int16_t)((float)phoneCx + wob), phoneCy, colors.eye, 255);
        drawVibrationArcs(gfx, phoneCx, phoneCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "shrink")) {
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        drawVibrationArcs(gfx, phoneCx, phoneCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "ringing")) {
        // Ringing continues with stronger vibration
        float wob = sinf(ph.p * TAU * 8.0f) * 7.0f;
        drawPhone(gfx, (int16_t)((float)phoneCx + wob), phoneCy, colors.eye, 255);
        drawVibrationArcs(gfx, phoneCx, phoneCy, ph.p * 2.0f, colors.eye);

        // "RING" text pulsing
        uint8_t textOp = (uint8_t)(sinf(ph.p * TAU * 4.0f) * 60.0f + 195.0f);
        gfx.drawText("RING", (int16_t)(SCX + 40), (int16_t)(phoneCy - 4), colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, textOp);
    } else if (phaseIs(ph.name, "answer")) {
        // Phone expands slightly (answered)
        float scaleP = 1.0f + sinf(ease::out(ph.p) * PI) * 0.15f;
        gfx.pushTransform();
        gfx.translate((float)phoneCx, (float)phoneCy);
        gfx.scale(scaleP, scaleP);
        gfx.translate(-(float)phoneCx, -(float)phoneCy);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        gfx.popTransform();
    } else if (phaseIs(ph.name, "talk")) {
        // Phone stays, EQ bars animate
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        drawVoiceBars(gfx, SCX + 20, phoneCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "hangup")) {
        // Phone shrinks
        float ep = ease::in(ph.p);
        float sc = lerp(1.0f, 0.7f, ep);
        gfx.pushTransform();
        gfx.translate((float)phoneCx, (float)phoneCy);
        gfx.scale(sc, sc);
        gfx.translate(-(float)phoneCx, -(float)phoneCy);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        gfx.popTransform();
    } else if (phaseIs(ph.name, "phoneOut")) {
        // Phone exits left
        float ep = ease::in(ph.p);
        phoneCx = (int16_t)lerp((float)(SCX - 40), -60.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "ring", "ringing")) {
        drawActLabel(gfx, "INCOMING", colors.eye);
    } else if (phaseIn(ph.name, "answer", "talk")) {
        drawActLabel(gfx, "ON CALL", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "CALL ENDED", colors.eye);
    }
}

/* ======================================================================== */
/*  Variant 2: call-outgoing  (13s, TONE_GREEN)                             */
/* ======================================================================== */

static const PhaseEntry OUTGOING_PHASES[] = {
    {"idle",     0.06f},   // Eager Luni
    {"phoneIn",  0.07f},   // Phone slides in
    {"shrink",   0.05f},   // Eyes shrink
    {"dial",     0.12f},   // Dial dots pulse
    {"wait",     0.20f},   // Ring arcs pulse (waiting)
    {"connect",  0.08f},   // Connected!
    {"talk",     0.16f},   // Voice bars
    {"bye",      0.06f},   // Bye
    {"phoneOut", 0.06f},   // Phone exits
    {"grow",     0.07f},   // Eyes grow back
    {"done",     0.07f},   // Happy Luni
};
static constexpr uint8_t OUT_PHASE_COUNT = sizeof(OUTGOING_PHASES) / sizeof(OUTGOING_PHASES[0]);

static void render_call_outgoing(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, OUTGOING_PHASES, OUT_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "phoneIn")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "dial", "wait")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "connect")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "talk")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIn(ph.name, "bye", "phoneOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "happy";
    }

    /* --- Prop: phone -------------------------------------------------- */
    int16_t phoneCx = SCX - 40, phoneCy = CY + 4;

    if (phaseIs(ph.name, "phoneIn")) {
        float ep = ease::out(ph.p);
        phoneCx = (int16_t)lerp(-50.0f, (float)(SCX - 40), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "dial")) {
        // Phone stays, dots pulse outward (dialing)
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        for (int i = 0; i < 3; i++) {
            float dp = fmodf(ph.p * 3.0f + (float)i * 0.33f, 1.0f);
            uint8_t op = (uint8_t)(sinf(dp * PI) * 220);
            int16_t dx = (int16_t)(SCX + 10 + (float)i * 18);
            gfx.fillCircle(dx, phoneCy, 4, colors.eye, op);
        }
    } else if (phaseIs(ph.name, "wait")) {
        // Waiting for answer: ring arcs pulse
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        drawRingDots(gfx, phoneCx, phoneCy, ph.p, colors.eye);

        // "CALLING" text
        uint8_t textOp = (uint8_t)(sinf(ph.p * TAU * 2.0f) * 50.0f + 205.0f);
        gfx.drawText("CALLING...", (int16_t)(SCX + 30), (int16_t)(phoneCy - 4), colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, textOp);
    } else if (phaseIs(ph.name, "connect")) {
        // Connected! Phone bounces
        float bounceP = sinf(ease::out(ph.p) * PI) * 0.12f;
        gfx.pushTransform();
        gfx.translate((float)phoneCx, (float)phoneCy);
        gfx.scale(1.0f + bounceP, 1.0f + bounceP);
        gfx.translate(-(float)phoneCx, -(float)phoneCy);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        gfx.popTransform();

        // "CONNECTED" text
        uint8_t cOp = (uint8_t)(ease::out(ph.p) * 255);
        gfx.drawText("CONNECTED!", (int16_t)(SCX + 30), phoneCy, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, cOp);
    } else if (phaseIs(ph.name, "talk")) {
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        drawVoiceBars(gfx, SCX + 20, phoneCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "bye")) {
        float ep = ease::in(ph.p);
        float sc = lerp(1.0f, 0.7f, ep);
        gfx.pushTransform();
        gfx.translate((float)phoneCx, (float)phoneCy);
        gfx.scale(sc, sc);
        gfx.translate(-(float)phoneCx, -(float)phoneCy);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        gfx.popTransform();
    } else if (phaseIs(ph.name, "phoneOut")) {
        float ep = ease::in(ph.p);
        phoneCx = (int16_t)lerp((float)(SCX - 40), -60.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "dial", "wait")) {
        drawActLabel(gfx, "DIALING", colors.eye);
    } else if (phaseIn(ph.name, "connect", "talk")) {
        drawActLabel(gfx, "ON CALL", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "BYE!", colors.eye);
    }
}

/* ======================================================================== */
/*  Variant 3: call-missed  (12s, TONE_RED)                                 */
/* ======================================================================== */

static const PhaseEntry MISSED_PHASES[] = {
    {"idle",     0.06f},   // Sleepy Luni
    {"phoneIn",  0.07f},   // Phone slides in
    {"ring",     0.14f},   // Ring vibrates
    {"shrink",   0.05f},   // Eyes shrink
    {"ringing",  0.22f},   // Ringing (ignored)
    {"fade",     0.10f},   // Ring fades away
    {"missed",   0.12f},   // "MISSED" X mark
    {"phoneOut", 0.06f},   // Phone exits
    {"grow",     0.07f},   // Eyes grow back
    {"done",     0.11f},   // Sad Luni
};
static constexpr uint8_t MISS_PHASE_COUNT = sizeof(MISSED_PHASES) / sizeof(MISSED_PHASES[0]);

static void render_call_missed(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, MISSED_PHASES, MISS_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "sleepy";
    } else if (phaseIn(ph.name, "phoneIn", "ring")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "sleepy";
    } else if (phaseIs(ph.name, "ringing")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "fade")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "missed")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "sad";
    } else if (phaseIs(ph.name, "phoneOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "sad";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "sad";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "sad";
    }

    /* --- Prop: phone -------------------------------------------------- */
    int16_t phoneCx = SCX - 40, phoneCy = CY + 4;

    if (phaseIs(ph.name, "phoneIn")) {
        float ep = ease::out(ph.p);
        phoneCx = (int16_t)lerp(-50.0f, (float)(SCX - 40), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "ring")) {
        // Ring vibrates
        float wob = sinf(ph.p * TAU * 5.0f) * 4.0f;
        drawPhone(gfx, (int16_t)((float)phoneCx + wob), phoneCy, colors.eye, 255);
        drawVibrationArcs(gfx, phoneCx, phoneCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "shrink")) {
        float wob = sinf(ph.p * TAU * 4.0f) * 3.0f;
        drawPhone(gfx, (int16_t)((float)phoneCx + wob), phoneCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "ringing")) {
        // Ringing continues but ignored
        float wob = sinf(ph.p * TAU * 6.0f) * (6.0f - ph.p * 2.0f);
        drawPhone(gfx, (int16_t)((float)phoneCx + wob), phoneCy, colors.eye, 255);
        drawVibrationArcs(gfx, phoneCx, phoneCy, ph.p * 2.0f, colors.eye);
    } else if (phaseIs(ph.name, "fade")) {
        // Ring fading out
        float fadeAlpha = 1.0f - ease::in(ph.p);
        uint8_t rOp = (uint8_t)(fadeAlpha * 180);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);
        // Fading arcs
        for (int i = 0; i < 3; i++) {
            float p = fmodf(ph.p + (float)i * 0.33f, 1.0f);
            int16_t r = (int16_t)(20.0f + p * 20.0f);
            gfx.drawArc((int16_t)(phoneCx + 18), phoneCy, r,
                        -0.5f, 0.5f, colors.eye, 2, rOp);
        }
    } else if (phaseIs(ph.name, "missed")) {
        // Phone with X mark, "MISSED" text
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, 255);

        // X mark over phone
        uint8_t xOp = (uint8_t)(ease::out(clamp(ph.p * 2.0f, 0.0f, 1.0f)) * 255);
        int16_t xCx = (int16_t)(phoneCx + 22), xCy = (int16_t)(phoneCy - 20);
        gfx.drawLine((int16_t)(xCx - 10), (int16_t)(xCy - 10),
                     (int16_t)(xCx + 10), (int16_t)(xCy + 10), colors.eye, 4, xOp);
        gfx.drawLine((int16_t)(xCx + 10), (int16_t)(xCy - 10),
                     (int16_t)(xCx - 10), (int16_t)(xCy + 10), colors.eye, 4, xOp);

        // "MISSED" text
        uint8_t blinkOp = ((int)(ph.p * 4.0f) % 2 == 0) ? (uint8_t)255 : (uint8_t)160;
        gfx.drawText("MISSED", (int16_t)(SCX + 40), phoneCy, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, blinkOp);
    } else if (phaseIs(ph.name, "phoneOut")) {
        float ep = ease::in(ph.p);
        phoneCx = (int16_t)lerp((float)(SCX - 40), -60.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawPhone(gfx, phoneCx, phoneCy, colors.eye, alpha);
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "ring", "ringing")) {
        drawActLabel(gfx, "RINGING", colors.eye);
    } else if (phaseIs(ph.name, "missed")) {
        drawActLabel(gfx, "MISSED CALL", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "OH NO", colors.eye);
    }
}

/* ======================================================================== */
/*  Registration                                                            */
/* ======================================================================== */

const VariantDef CALL_VARIANTS[] = {
    {"call-incoming", "Incoming", 14000, TONE_NONE, render_call_incoming},
    {"call-outgoing", "Outgoing", 13000, TONE_NONE, render_call_outgoing},
    {"call-missed",   "Missed",   12000, TONE_RED,  render_call_missed},
};

extern const CategoryDef CAT_CALL = {
    "call", "Call", ContentKind::SCENE, TONE_GREEN,
    CALL_VARIANTS, sizeof(CALL_VARIANTS) / sizeof(CALL_VARIANTS[0]), false
};
