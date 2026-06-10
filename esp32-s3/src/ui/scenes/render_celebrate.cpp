#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry CELEB_PHASES[] = {
    {"curious",   0.06f},   // Luni curious, centered
    {"drumroll",  0.06f},   // Pulsing dots build anticipation
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"trophyUp",  0.08f},   // Trophy rises from bottom
    {"sparks",    0.08f},   // Spark burst around trophy
    {"confetti",  0.14f},   // Confetti falls from top
    {"firework",  0.18f},   // 3 phased firework bursts
    {"bow",       0.06f},   // Trophy dips (bow)
    {"trophyDn",  0.06f},   // Trophy descends off-screen
    {"grow",      0.07f},   // Eyes grow back to center
    {"done",      0.16f},   // Excited Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(CELEB_PHASES) / sizeof(CELEB_PHASES[0]);

/* -- Helper: draw trophy ------------------------------------------------- */
static void drawTrophy(GfxEngine& gfx, int16_t cx, int16_t cy,
                       uint16_t color, uint8_t alpha) {
    // Cup body (trapezoid via two triangles)
    gfx.fillTriangle((int16_t)(cx - 24), (int16_t)(cy - 28),
                     (int16_t)(cx + 24), (int16_t)(cy - 28),
                     (int16_t)(cx + 18), (int16_t)(cy + 4), color, alpha);
    gfx.fillTriangle((int16_t)(cx - 24), (int16_t)(cy - 28),
                     (int16_t)(cx + 18), (int16_t)(cy + 4),
                     (int16_t)(cx - 18), (int16_t)(cy + 4), color, alpha);
    // Stem
    gfx.fillRoundedRect((int16_t)(cx - 6), (int16_t)(cy + 4), 12, 14, 2, color, alpha);
    // Base
    gfx.fillRoundedRect((int16_t)(cx - 20), (int16_t)(cy + 16), 40, 6, 2, color, alpha);
    // Handles (bezier arcs)
    gfx.pushAlpha(alpha);
    gfx.drawQuadBezier((int16_t)(cx - 24), (int16_t)(cy - 20),
                       (int16_t)(cx - 38), (int16_t)(cy - 14),
                       (int16_t)(cx - 34), (int16_t)(cy + 2), color, 3);
    gfx.drawQuadBezier((int16_t)(cx + 24), (int16_t)(cy - 20),
                       (int16_t)(cx + 38), (int16_t)(cy - 14),
                       (int16_t)(cx + 34), (int16_t)(cy + 2), color, 3);
    gfx.popAlpha();
}

/* -- Main render --------------------------------------------------------- */
static void render_celebrate(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, CELEB_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "curious")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "drumroll")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "trophyUp", "sparks")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIn(ph.name, "confetti", "firework")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIn(ph.name, "bow", "trophyDn")) {
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

    /* --- Prop: trophy ------------------------------------------------- */
    int16_t trophyCx = SCX, trophyCy = CY + 6;

    if (phaseIs(ph.name, "drumroll")) {
        // Pulsing dots (anticipation)
        for (int i = 0; i < 3; i++) {
            float dp = sinf(ph.p * TAU * 3.0f + (float)i * 1.2f);
            uint8_t op = (uint8_t)(clamp(dp, 0.0f, 1.0f) * 200);
            gfx.fillCircle((int16_t)(SCX - 20 + i * 20), CY + 30, 5, colors.eye, op);
        }
    } else if (phaseIs(ph.name, "trophyUp")) {
        // Trophy rises from bottom
        float ep = ease::out(ph.p);
        trophyCy = (int16_t)lerp((float)(SCREEN_H + 40), (float)(CY + 6), ep);
        drawTrophy(gfx, trophyCx, trophyCy, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "sparks")) {
        drawTrophy(gfx, trophyCx, trophyCy, colors.eye, 255);
        // Spark lines radiating from trophy
        for (int i = 0; i < 8; i++) {
            float a = (float)i / 8.0f * TAU;
            float r1 = 44.0f + ph.p * 10.0f;
            float r2 = r1 + 14.0f;
            uint8_t op = (uint8_t)((1.0f - ph.p * 0.5f) * 220);
            gfx.drawLine((int16_t)(trophyCx + cosf(a) * r1),
                         (int16_t)(trophyCy + sinf(a) * r1),
                         (int16_t)(trophyCx + cosf(a) * r2),
                         (int16_t)(trophyCy + sinf(a) * r2),
                         colors.eye, 2, op);
        }
    } else if (phaseIs(ph.name, "confetti")) {
        drawTrophy(gfx, trophyCx, trophyCy, colors.eye, 255);
        // Confetti falling
        static const ToneId CONF_TONES[] = {
            TONE_WARM, TONE_ROSE, TONE_CYAN, TONE_GREEN, TONE_PURPLE, TONE_ORANGE
        };
        for (int i = 0; i < 18; i++) {
            float ip = fmodf(ph.p * 1.5f + (float)i * 0.055f, 1.0f);
            int16_t x = (int16_t)((i * 53 + (int)(sinf(ip * TAU + (float)i) * 8.0f)) % SCREEN_W);
            int16_t y = (int16_t)lerp((float)(STATUS_H + 8), (float)(SCREEN_H - 8), ip);
            uint8_t op = (uint8_t)((1.0f - ip) * 230);
            uint16_t pc = colors.tones[CONF_TONES[i % 6]];
            int shape = i % 3;
            if (shape == 0) {
                gfx.fillRect((int16_t)(x - 3), (int16_t)(y - 2), 6, 4, pc, op);
            } else if (shape == 1) {
                gfx.fillCircle(x, y, 3, pc, op);
            } else {
                gfx.fillRect((int16_t)(x - 2), (int16_t)(y - 4), 3, 8, pc, op);
            }
        }
    } else if (phaseIs(ph.name, "firework")) {
        drawTrophy(gfx, trophyCx, trophyCy, colors.eye, 255);
        // 3 phased firework bursts
        struct Burst { int16_t x, y; float phase; ToneId tone; };
        static const Burst BURSTS[] = {
            {(int16_t)(SCX - 60), (int16_t)(CY - 20), 0.0f,  TONE_WARM},
            {(int16_t)(SCX + 55), (int16_t)(CY + 10), 0.33f, TONE_ROSE},
            {SCX,                 (int16_t)(CY - 36),  0.66f, TONE_GREEN},
        };
        for (int b = 0; b < 3; b++) {
            float bp = ph.p - BURSTS[b].phase;
            if (bp < 0.0f) continue;
            bp = clamp(bp / 0.34f, 0.0f, 1.0f);
            float r = lerp(2.0f, 46.0f, bp);
            uint8_t op = (uint8_t)((1.0f - bp) * 255);
            uint16_t bc = colors.tones[BURSTS[b].tone];
            for (int j = 0; j < 10; j++) {
                float a = (float)j / 10.0f * TAU;
                gfx.drawLine(
                    (int16_t)(BURSTS[b].x + cosf(a) * r * 0.5f),
                    (int16_t)(BURSTS[b].y + sinf(a) * r * 0.5f),
                    (int16_t)(BURSTS[b].x + cosf(a) * r),
                    (int16_t)(BURSTS[b].y + sinf(a) * r),
                    bc, 3, op);
            }
            int16_t dotR = (int16_t)(3.0f + (1.0f - bp) * 3.0f);
            gfx.fillCircle(BURSTS[b].x, BURSTS[b].y, dotR, bc, op);
        }
    } else if (phaseIs(ph.name, "bow")) {
        // Trophy dips down then back (bow)
        float dip = sinf(ph.p * PI) * 16.0f;
        drawTrophy(gfx, trophyCx, (int16_t)(trophyCy + dip), colors.eye, 255);
    } else if (phaseIs(ph.name, "trophyDn")) {
        // Trophy descends off-screen
        float ep = ease::in(ph.p);
        trophyCy = (int16_t)lerp((float)(CY + 6), (float)(SCREEN_H + 50), ep);
        drawTrophy(gfx, trophyCx, trophyCy, colors.eye, (uint8_t)((1.0f - ep) * 255));
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "trophyUp", "sparks")) {
        drawActLabel(gfx, "TROPHY", colors.eye);
    } else if (phaseIn(ph.name, "confetti", "firework")) {
        drawActLabel(gfx, "CELEBRATE!", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "CHAMPION", colors.eye);
    }
}

const VariantDef CELEBRATE_VARIANTS[] = {
    {"celebrate-grand", "Grand", 14000, TONE_NONE, render_celebrate},
};

extern const CategoryDef CAT_CELEBRATE = {
    "celebrate", "Celebrate", ContentKind::SCENE, TONE_CYAN,
    CELEBRATE_VARIANTS, sizeof(CELEBRATE_VARIANTS) / sizeof(CELEBRATE_VARIANTS[0]), false
};
