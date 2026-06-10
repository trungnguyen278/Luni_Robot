#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry REMINDER_PHASES[] = {
    {"idle",       0.05f},   // Luni bored/sleepy, centered
    {"ding",       0.06f},   // Bell icon appears, ding!
    {"shrink",     0.05f},   // Eyes shrink to top-right corner
    {"glassIn",    0.06f},   // Water glass slides in from left
    {"drink",      0.10f},   // Glass tips, water "drinks"
    {"glassOut",   0.05f},   // Glass slides out right
    {"pillIn",     0.06f},   // Pill bottle slides in from right
    {"take",       0.10f},   // Pill pops out, taken
    {"pillOut",    0.05f},   // Bottle slides out left
    {"stretchIn",  0.06f},   // Stretch figure fades in
    {"hold",       0.12f},   // Stretch pose held, gentle pulse
    {"stretchOut", 0.06f},   // Figure fades out
    {"grow",       0.07f},   // Eyes grow back to center
    {"done",       0.11f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(REMINDER_PHASES) / sizeof(REMINDER_PHASES[0]);

/* ── Helper: draw bell icon ──────────────────────────────────────── */
static void drawBellIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float scale, uint16_t color, uint8_t alpha) {
    int16_t r = (int16_t)(10.0f * scale);
    gfx.fillCircle(cx, (int16_t)(cy - r), r, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - r), cy, (int16_t)(r * 2), (int16_t)(r * 0.8f),
                        2, color, alpha);
    gfx.fillCircle(cx, (int16_t)(cy + r), (int16_t)(3.0f * scale), color, alpha);
    gfx.drawArc(cx, (int16_t)(cy - r * 2), (int16_t)(4.0f * scale),
                PI, TAU, color, 2, alpha);
}

/* ── Helper: draw water glass ────────────────────────────────────── */
static void drawWaterGlass(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float tilt, float waterLevel,
                           uint16_t color, uint8_t alpha) {
    gfx.pushTransform();
    gfx.rotate(tilt, (float)cx, (float)cy);

    // Glass body (rect)
    gfx.strokeRoundedRect((int16_t)(cx - 14), (int16_t)(cy - 24),
                          28, 48, 3, color, 2);

    // Water fill (inside rect, from bottom up)
    int16_t fillH = (int16_t)(40.0f * waterLevel);
    if (fillH > 0) {
        gfx.fillRoundedRect((int16_t)(cx - 12), (int16_t)(cy + 22 - fillH),
                            24, fillH, 2, color, (uint8_t)(alpha * 0.6f));
    }

    // Trapezoid rim (wider at top)
    gfx.drawLine((int16_t)(cx - 16), (int16_t)(cy - 26),
                 (int16_t)(cx + 16), (int16_t)(cy - 26), color, 2, alpha);

    gfx.popTransform();
}

/* ── Helper: draw pill bottle ────────────────────────────────────── */
static void drawPillBottle(GfxEngine& gfx, int16_t cx, int16_t cy,
                           uint16_t color, uint8_t alpha) {
    // Cap
    gfx.fillRoundedRect((int16_t)(cx - 10), (int16_t)(cy - 30),
                        20, 10, 3, color, alpha);
    // Body
    gfx.fillRoundedRect((int16_t)(cx - 14), (int16_t)(cy - 20),
                        28, 44, 4, color, alpha);
    // Label stripe
    gfx.fillRect((int16_t)(cx - 12), (int16_t)(cy - 6),
                 24, 14, COLOR_BG, (uint8_t)(alpha * 0.5f));
    // Plus sign on label
    gfx.drawLine((int16_t)(cx - 5), cy, (int16_t)(cx + 5), cy, color, 2, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 5), cx, (int16_t)(cy + 5), color, 2, alpha);
}

/* ── Helper: draw pill capsule ───────────────────────────────────── */
static void drawPill(GfxEngine& gfx, int16_t cx, int16_t cy,
                     uint16_t color, uint8_t alpha) {
    gfx.fillCircle((int16_t)(cx - 4), cy, 4, color, alpha);
    gfx.fillCircle((int16_t)(cx + 4), cy, 4, color, alpha);
    gfx.fillRect((int16_t)(cx - 4), (int16_t)(cy - 4), 8, 8, color, alpha);
}

/* ── Helper: draw stick figure in stretch pose ───────────────────── */
static void drawStretchFigure(GfxEngine& gfx, int16_t cx, int16_t cy,
                              float breathe, uint16_t color, uint8_t alpha) {
    int16_t headR = 8;
    int16_t headY = (int16_t)(cy - 36 - breathe * 2.0f);

    // Head
    gfx.strokeCircle(cx, headY, headR, color, 2);

    // Body
    int16_t bodyTop = (int16_t)(headY + headR + 2);
    int16_t bodyBot = (int16_t)(cy + 6);
    gfx.drawLine(cx, bodyTop, cx, bodyBot, color, 3, alpha);

    // Arms raised (stretching up)
    float armSpread = 30.0f + breathe * 4.0f;
    gfx.drawLine(cx, (int16_t)(bodyTop + 8),
                 (int16_t)(cx - armSpread), (int16_t)(bodyTop - 14), color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(bodyTop + 8),
                 (int16_t)(cx + armSpread), (int16_t)(bodyTop - 14), color, 3, alpha);

    // Legs
    gfx.drawLine(cx, bodyBot,
                 (int16_t)(cx - 16), (int16_t)(bodyBot + 24), color, 3, alpha);
    gfx.drawLine(cx, bodyBot,
                 (int16_t)(cx + 16), (int16_t)(bodyBot + 24), color, 3, alpha);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_reminder(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, REMINDER_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "sleepy";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "ding")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "glassIn", "drink", "glassOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIn(ph.name, "pillIn", "take", "pillOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIn(ph.name, "stretchIn", "hold", "stretchOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Prop: bell icon (ding phase) ----------------------------- */
    if (phaseIs(ph.name, "ding")) {
        float bellScale = ease::out(ph.p) * 1.2f;
        uint8_t bellAlpha = (uint8_t)(ease::out(ph.p) * 255);
        float shake = sinf(ph.p * TAU * 4.0f) * 6.0f * (1.0f - ph.p);
        drawBellIcon(gfx, (int16_t)(SCX + shake), STATUS_H + 40,
                     bellScale, colors.eye, bellAlpha);
    }

    /* --- Vignette 1: Water glass ---------------------------------- */
    int16_t propCy = CY + 10;
    if (phaseIs(ph.name, "glassIn")) {
        float ep = ease::out(ph.p);
        int16_t gx = (int16_t)lerp(-40.0f, (float)SCX, ep);
        drawWaterGlass(gfx, gx, propCy, 0.0f, 0.8f, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "drink")) {
        float tilt = lerp(0.0f, -25.0f, ease::inOut(ph.p));
        float level = lerp(0.8f, 0.1f, ease::inOut(ph.p));
        drawWaterGlass(gfx, SCX, propCy, tilt, level, colors.eye, 255);
        // Drinking text
        if (ph.p > 0.3f && ph.p < 0.8f) {
            gfx.drawText("HYDRATE", SCX, STATUS_H + 28, colors.eye, 1,
                         GfxEngine::TextAlign::CENTER, 178);
        }
    } else if (phaseIs(ph.name, "glassOut")) {
        float ep = ease::in(ph.p);
        int16_t gx = (int16_t)lerp((float)SCX, (float)(SCREEN_W + 40), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawWaterGlass(gfx, gx, propCy, 0.0f, 0.1f, colors.eye, alpha);
    }

    /* --- Vignette 2: Pill bottle ---------------------------------- */
    if (phaseIs(ph.name, "pillIn")) {
        float ep = ease::out(ph.p);
        int16_t px = (int16_t)lerp((float)(SCREEN_W + 40), (float)SCX, ep);
        drawPillBottle(gfx, px, propCy, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "take")) {
        drawPillBottle(gfx, SCX, propCy, colors.eye, 255);
        // Pill pops out and floats up
        float pillP = ease::out(clamp(ph.p * 2.0f, 0.0f, 1.0f));
        int16_t pillX = (int16_t)(SCX + 20.0f + pillP * 16.0f);
        int16_t pillY = (int16_t)(propCy - 20.0f - pillP * 30.0f);
        float fadeP = ph.p > 0.6f ? (ph.p - 0.6f) / 0.4f : 0.0f;
        uint8_t pillAlpha = (uint8_t)((1.0f - fadeP) * 255);
        drawPill(gfx, pillX, pillY, colors.eye, pillAlpha);
        if (ph.p > 0.4f) {
            gfx.drawText("MEDS", SCX, STATUS_H + 28, colors.eye, 1,
                         GfxEngine::TextAlign::CENTER, 178);
        }
    } else if (phaseIs(ph.name, "pillOut")) {
        float ep = ease::in(ph.p);
        int16_t px = (int16_t)lerp((float)SCX, -40.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawPillBottle(gfx, px, propCy, colors.eye, alpha);
    }

    /* --- Vignette 3: Stretch figure ------------------------------- */
    if (phaseIs(ph.name, "stretchIn")) {
        float ep = ease::out(ph.p);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawStretchFigure(gfx, SCX, propCy, 0.0f, colors.eye, alpha);
    } else if (phaseIs(ph.name, "hold")) {
        float breathe = sinf(ph.p * TAU * 2.0f) * 4.0f;
        drawStretchFigure(gfx, SCX, propCy, breathe, colors.eye, 255);
        gfx.drawText("STRETCH", SCX, STATUS_H + 28, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 178);
    } else if (phaseIs(ph.name, "stretchOut")) {
        float ep = ease::in(ph.p);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawStretchFigure(gfx, SCX, propCy, 0.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "glassIn", "drink", "glassOut",
                "pillIn")) {
        drawActLabel(gfx, "REMINDER", colors.eye);
    } else if (phaseIn(ph.name, "take", "pillOut")) {
        drawActLabel(gfx, "REMINDER", colors.eye);
    } else if (phaseIn(ph.name, "stretchIn", "hold", "stretchOut")) {
        drawActLabel(gfx, "REMINDER", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ALL DONE", colors.eye);
    }
}

const VariantDef REMINDER_VARIANTS[] = {
    {"reminder-triple", "Water meds stretch", 14000, TONE_NONE, render_reminder},
};

extern const CategoryDef CAT_REMINDER = {
    "reminder", "Reminder", ContentKind::SCENE, TONE_RED,
    REMINDER_VARIANTS, sizeof(REMINDER_VARIANTS) / sizeof(REMINDER_VARIANTS[0]), false
};
