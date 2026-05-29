#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry MORN_PHASES[] = {
    {"asleep",  0.07f},   // Luni sleepy, Z's float up
    {"alarm",   0.06f},   // Bell shakes
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"sunPeek", 0.10f},   // Sun peeks from bottom
    {"sunUp",   0.14f},   // Sun rises, rays extend
    {"stretch", 0.12f},   // Stick figure stretches
    {"coffee",  0.12f},   // Coffee cup appears with steam
    {"ready",   0.10f},   // Door opens, figure exits
    {"sunOut",  0.06f},   // Sun exits upward
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.11f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(MORN_PHASES) / sizeof(MORN_PHASES[0]);

/* -- Helper: draw sun with rays ------------------------------------------ */
static void drawSun(GfxEngine& gfx, int16_t cx, int16_t cy, float rayLen,
                    uint16_t color, uint8_t alpha) {
    // Sun disc
    gfx.fillCircle(cx, cy, 24, color, alpha);
    // Rays
    if (rayLen > 0.0f) {
        for (int i = 0; i < 12; i++) {
            float a = (float)i / 12.0f * TAU;
            float r1 = 28.0f;
            float r2 = 28.0f + rayLen;
            gfx.drawLine((int16_t)(cx + cosf(a) * r1), (int16_t)(cy + sinf(a) * r1),
                         (int16_t)(cx + cosf(a) * r2), (int16_t)(cy + sinf(a) * r2),
                         color, 2, alpha);
        }
    }
}

/* -- Helper: draw alarm bell --------------------------------------------- */
static void drawAlarmBell(GfxEngine& gfx, int16_t cx, int16_t cy, float shake,
                          uint16_t color, uint8_t alpha) {
    int16_t sx = (int16_t)shake;
    // Dome
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy - 14), 16, color, alpha);
    // Body
    gfx.fillRoundedRect((int16_t)(cx - 14 + sx), (int16_t)(cy - 4), 28, 18, 3, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - 18 + sx), (int16_t)(cy + 10), 36, 6, 3, color, alpha);
    // Clapper
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy + 20), 3, color, alpha);
}

/* -- Helper: draw stretch figure ----------------------------------------- */
static void drawStretchFigure(GfxEngine& gfx, int16_t cx, int16_t cy,
                              float stretchP, uint16_t color, uint8_t alpha) {
    // Head
    gfx.fillCircle(cx, (int16_t)(cy - 28), 7, color, alpha);
    // Body
    gfx.drawLine(cx, (int16_t)(cy - 21), cx, (int16_t)(cy - 2), color, 3, alpha);
    // Arms reach up during stretch
    float armUp = sinf(stretchP * PI) * 30.0f;
    gfx.drawLine(cx, (int16_t)(cy - 16),
                 (int16_t)(cx - 14), (int16_t)(cy - 16.0f - armUp),
                 color, 2, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 16),
                 (int16_t)(cx + 14), (int16_t)(cy - 16.0f - armUp),
                 color, 2, alpha);
    // Legs
    gfx.drawLine(cx, (int16_t)(cy - 2),
                 (int16_t)(cx - 8), cy, color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 2),
                 (int16_t)(cx + 8), cy, color, 3, alpha);
}

/* -- Helper: draw coffee cup with steam ---------------------------------- */
static void drawCoffeeCup(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float steamP, uint16_t color, uint8_t alpha) {
    // Cup body
    gfx.fillRoundedRect((int16_t)(cx - 12), cy, 24, 20, 3, color, alpha);
    // Handle
    gfx.drawQuadBezier((int16_t)(cx + 12), (int16_t)(cy + 4),
                       (int16_t)(cx + 22), (int16_t)(cy + 10),
                       (int16_t)(cx + 12), (int16_t)(cy + 16), color, 2);
    // Steam wisps
    for (int i = 0; i < 3; i++) {
        float sp = fmodf(steamP + (float)i * 0.33f, 1.0f);
        float wx = sinf(sp * TAU * 2.0f + (float)i) * 4.0f;
        int16_t sy = (int16_t)(cy - 4.0f - sp * 18.0f);
        uint8_t op = (uint8_t)((1.0f - sp) * 160);
        gfx.drawLine((int16_t)(cx - 6 + i * 6 + (int16_t)wx), sy,
                     (int16_t)(cx - 6 + i * 6 + (int16_t)(wx * 0.5f)),
                     (int16_t)(sy - 6), color, 2, op);
    }
}

/* -- Helper: draw door --------------------------------------------------- */
static void drawDoor(GfxEngine& gfx, int16_t cx, int16_t cy, float openP,
                     uint16_t color, uint8_t alpha) {
    // Door frame
    gfx.strokeRoundedRect((int16_t)(cx - 16), (int16_t)(cy - 30), 32, 56, 2, color, 2);
    // Door panel (narrows as it opens)
    float doorW = 28.0f * (1.0f - openP * 0.7f);
    gfx.fillRoundedRect((int16_t)(cx - 14), (int16_t)(cy - 28),
                        (int16_t)doorW, 52, 2, color, (uint8_t)(alpha * 0.3f));
    // Knob
    gfx.fillCircle((int16_t)(cx - 14 + (int16_t)doorW - 6), (int16_t)(cy + 2),
                   3, color, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_morning(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, MORN_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "asleep")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "alarm")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "surprised";
    } else if (phaseIn(ph.name, "sunPeek", "sunUp")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "stretch")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "coffee")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "ready")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "sunOut")) {
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
    int16_t sunCx = SCX - 30;
    int16_t horizonY = SCREEN_H - 24;

    if (phaseIs(ph.name, "asleep")) {
        // Floating Z letters
        for (int i = 0; i < 3; i++) {
            float zp = fmodf(ph.p * 2.0f + (float)i * 0.33f, 1.0f);
            int16_t zx = (int16_t)(SCX + 30 + i * 12);
            int16_t zy = (int16_t)(CY - 10.0f - zp * 40.0f);
            uint8_t op = (uint8_t)((1.0f - zp) * 200);
            float zScale = 1.0f + (float)i * 0.3f;
            gfx.drawText("Z", zx, zy, colors.eye, (int16_t)zScale,
                         GfxEngine::TextAlign::CENTER, op);
        }
    } else if (phaseIs(ph.name, "alarm")) {
        // Bell shaking
        float shake = sinf(ph.p * TAU * 10.0f) * (4.0f + ph.p * 8.0f);
        drawAlarmBell(gfx, SCX, CY + 4, shake, colors.eye, 255);
        // Sound wave arcs
        for (int i = 0; i < 2; i++) {
            float wp = fmodf(ph.p * 3.0f + (float)i * 0.5f, 1.0f);
            uint8_t op = (uint8_t)((1.0f - wp) * 140);
            int16_t wr = (int16_t)(24.0f + wp * 20.0f);
            gfx.drawArc((int16_t)(SCX - 28), CY + 4, wr,
                        -0.5f, 0.5f, colors.eye, 2, op);
            gfx.drawArc((int16_t)(SCX + 28), CY + 4, wr,
                        PI - 0.5f, PI + 0.5f, colors.eye, 2, op);
        }
    } else if (phaseIs(ph.name, "shrink")) {
        // Horizon line appears
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2,
                     (uint8_t)(ph.p * 255));
    } else if (phaseIs(ph.name, "sunPeek")) {
        // Sun peeks from bottom
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);
        float ep = ease::out(ph.p);
        int16_t sunY = (int16_t)lerp((float)(horizonY + 30), (float)horizonY, ep);
        gfx.fillCircle(sunCx, sunY, 24, colors.eye, (uint8_t)(ep * 255));
        // Below-horizon mask
        gfx.fillRect(0, (int16_t)(horizonY + 1), SCREEN_W,
                     (int16_t)(SCREEN_H - horizonY), colors.bg);
    } else if (phaseIs(ph.name, "sunUp")) {
        // Sun rises with rays extending
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);
        float ep = ease::out(ph.p);
        int16_t sunY = (int16_t)lerp((float)horizonY, (float)(CY + 10), ep);
        float rayLen = ep * 20.0f;
        drawSun(gfx, sunCx, sunY, rayLen, colors.eye, 255);
        // Below-horizon mask for partial rise
        if (sunY > horizonY - 30) {
            gfx.fillRect(0, (int16_t)(horizonY + 1), SCREEN_W,
                         (int16_t)(SCREEN_H - horizonY), colors.bg);
        }
    } else if (phaseIs(ph.name, "stretch")) {
        // Sun in position + stretch figure
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);
        drawSun(gfx, sunCx, CY + 10, 20.0f, colors.eye, 200);
        // Stretch figure on right side
        int16_t figGroundY = horizonY;
        drawStretchFigure(gfx, SCX + 50, figGroundY, ph.p, colors.eye, 255);
    } else if (phaseIs(ph.name, "coffee")) {
        // Sun + coffee cup appears
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);
        drawSun(gfx, sunCx, CY + 10, 20.0f, colors.eye, 180);

        float ep = ease::out(clamp(ph.p / 0.3f, 0.0f, 1.0f));
        int16_t cupY = (int16_t)lerp((float)(SCREEN_H + 10), (float)(CY + 20), ep);
        drawCoffeeCup(gfx, SCX + 50, cupY, ph.p, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "ready")) {
        // Door opens, figure exits
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2);
        drawSun(gfx, sunCx, CY + 10, 20.0f, colors.eye, 160);

        // Door on right
        drawDoor(gfx, SCX + 50, CY + 16, ease::out(clamp(ph.p / 0.4f, 0.0f, 1.0f)),
                 colors.eye, 255);

        // Figure walks toward door and exits
        if (ph.p > 0.3f) {
            float walkP = (ph.p - 0.3f) / 0.7f;
            int16_t fx = (int16_t)lerp((float)(SCX - 20), (float)(SCX + 50), walkP);
            uint8_t fAlpha = (uint8_t)((1.0f - clamp((walkP - 0.7f) / 0.3f, 0.0f, 1.0f)) * 255);
            gfx.fillCircle(fx, (int16_t)(horizonY - 28), 7, colors.eye, fAlpha);
            gfx.drawLine(fx, (int16_t)(horizonY - 21), fx, (int16_t)(horizonY - 2),
                         colors.eye, 3, fAlpha);
            float legA = sinf(walkP * TAU * 4.0f) * 8.0f;
            gfx.drawLine(fx, (int16_t)(horizonY - 2),
                         (int16_t)(fx - 6.0f + legA), horizonY,
                         colors.eye, 2, fAlpha);
            gfx.drawLine(fx, (int16_t)(horizonY - 2),
                         (int16_t)(fx + 6.0f - legA), horizonY,
                         colors.eye, 2, fAlpha);
        }
    } else if (phaseIs(ph.name, "sunOut")) {
        // Sun exits upward
        float ep = ease::in(ph.p);
        int16_t sunY = (int16_t)lerp((float)(CY + 10), (float)(STATUS_H - 40), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawSun(gfx, sunCx, sunY, 20.0f, colors.eye, alpha);
        gfx.drawLine(0, horizonY, SCREEN_W, horizonY, colors.eye, 2, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_WARM], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "sunPeek", "sunUp")) {
        drawActLabel(gfx, "SUNRISE", colors.eye);
    } else if (phaseIs(ph.name, "stretch")) {
        drawActLabel(gfx, "STRETCH", colors.eye);
    } else if (phaseIs(ph.name, "coffee")) {
        drawActLabel(gfx, "COFFEE TIME", colors.eye);
    } else if (phaseIs(ph.name, "ready")) {
        drawActLabel(gfx, "READY TO GO", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "GOOD MORNING", colors.eye);
    }
}

const VariantDef MORNING_VARIANTS[] = {
    {"morning-rise", "Morning Rise", 14000, TONE_NONE, render_morning},
};

extern const CategoryDef CAT_MORNING = {
    "morning", "Morning", ContentKind::SCENE, TONE_WARM,
    MORNING_VARIANTS, sizeof(MORNING_VARIANTS) / sizeof(MORNING_VARIANTS[0]), false
};
