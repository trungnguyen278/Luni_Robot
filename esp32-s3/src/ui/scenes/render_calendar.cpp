#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry CAL_PHASES[] = {
    {"idle",      0.06f},   // Luni steady, centered
    {"pageIn",    0.07f},   // Calendar page rises from bottom
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"flip1",     0.10f},   // Page flip: JAN
    {"flip2",     0.10f},   // Page flip: FEB
    {"flip3",     0.10f},   // Page flip: MAR
    {"land",      0.06f},   // Land on target page
    {"highlight", 0.10f},   // Date "22" highlights with glow
    {"ping",      0.08f},   // Bell pings with expanding rings
    {"tear",      0.08f},   // Page tears off upward
    {"grow",      0.07f},   // Eyes grow back to center
    {"done",      0.13f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(CAL_PHASES) / sizeof(CAL_PHASES[0]);

/* -- Helper: draw calendar page ------------------------------------------ */
static void drawCalendarPage(GfxEngine& gfx, int16_t cx, int16_t cy,
                             const char* month, uint16_t color, uint8_t alpha) {
    // Page body
    gfx.strokeRoundedRect((int16_t)(cx - 56), (int16_t)(cy - 40), 112, 90, 4, color, 2);
    // Header bar (filled)
    gfx.fillRoundedRect((int16_t)(cx - 56), (int16_t)(cy - 40), 112, 22, 4, color, alpha);
    // Month text on header
    gfx.drawText(month, cx, (int16_t)(cy - 26), COLOR_BG, 1,
                 GfxEngine::TextAlign::CENTER);
    // Calendar rings
    gfx.fillCircle((int16_t)(cx - 28), (int16_t)(cy - 42), 3, COLOR_BG);
    gfx.strokeCircle((int16_t)(cx - 28), (int16_t)(cy - 42), 3, color, 2);
    gfx.fillCircle((int16_t)(cx + 28), (int16_t)(cy - 42), 3, COLOR_BG);
    gfx.strokeCircle((int16_t)(cx + 28), (int16_t)(cy - 42), 3, color, 2);
}

/* -- Helper: draw simple date grid --------------------------------------- */
static void drawDateGrid(GfxEngine& gfx, int16_t cx, int16_t cy,
                         int16_t highlightDay, float glowP,
                         uint16_t color, uint16_t bgColor, uint8_t alpha) {
    // 7 columns x 4 rows simplified grid
    int16_t startX = (int16_t)(cx - 42);
    int16_t startY = (int16_t)(cy - 12);
    int16_t cellW = 14;
    int16_t cellH = 14;
    int day = 1;

    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 7 && day <= 28; col++) {
            int16_t dx = (int16_t)(startX + col * cellW);
            int16_t dy = (int16_t)(startY + row * cellH);

            if (day == highlightDay && glowP > 0.0f) {
                // Glow circle behind highlighted day
                float glowR = 8.0f + glowP * 6.0f;
                uint8_t glowAlpha = (uint8_t)(glowP * 180);
                gfx.fillCircle((int16_t)(dx + 4), (int16_t)(dy + 4),
                               (int16_t)glowR, color, glowAlpha);
                // Day number on top
                gfx.fillCircle((int16_t)(dx + 4), (int16_t)(dy + 4), 7, color, 255);
            } else {
                // Small dot for each day
                gfx.fillCircle((int16_t)(dx + 4), (int16_t)(dy + 4), 2, color,
                               (uint8_t)(alpha * 0.6f));
            }
            day++;
        }
    }
}

/* -- Helper: draw reminder bell ------------------------------------------ */
static void drawReminderBell(GfxEngine& gfx, int16_t cx, int16_t cy, float shake,
                             uint16_t color, uint8_t alpha) {
    int16_t sx = (int16_t)shake;
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy - 10), 12, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - 10 + sx), (int16_t)(cy - 2), 20, 14, 2, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - 14 + sx), (int16_t)(cy + 8), 28, 5, 2, color, alpha);
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(cy + 16), 3, color, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_calendar(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, CAL_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "pageIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "flip1", "flip2", "flip3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "land")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "highlight")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "ping")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "tear")) {
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
    int16_t pageCx = SCX, pageCy = CY + 10;

    if (phaseIs(ph.name, "pageIn")) {
        // Calendar rises from bottom
        float ep = ease::out(ph.p);
        int16_t py = (int16_t)lerp((float)(SCREEN_H + 50), (float)pageCy, ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawCalendarPage(gfx, pageCx, py, "JAN", colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawCalendarPage(gfx, pageCx, pageCy, "JAN", colors.eye, 255);
        drawDateGrid(gfx, pageCx, pageCy, -1, 0.0f, colors.eye, colors.bg, 255);
    } else if (phaseIs(ph.name, "flip1")) {
        // Page flip JAN -- sliding effect
        float flipP = ease::inOut(ph.p);
        // Old page slides up
        int16_t oldY = (int16_t)((float)pageCy - flipP * 30.0f);
        uint8_t oldAlpha = (uint8_t)((1.0f - flipP) * 255);
        if (oldAlpha > 20) {
            drawCalendarPage(gfx, pageCx, oldY, "JAN", colors.eye, oldAlpha);
        }
        // New page slides in from below
        int16_t newY = (int16_t)lerp((float)(pageCy + 30), (float)pageCy, flipP);
        uint8_t newAlpha = (uint8_t)(flipP * 255);
        drawCalendarPage(gfx, pageCx, newY, "FEB", colors.eye, newAlpha);
        drawDateGrid(gfx, pageCx, newY, -1, 0.0f, colors.eye, colors.bg, newAlpha);
    } else if (phaseIs(ph.name, "flip2")) {
        // Page flip FEB -> MAR
        float flipP = ease::inOut(ph.p);
        int16_t oldY = (int16_t)((float)pageCy - flipP * 30.0f);
        uint8_t oldAlpha = (uint8_t)((1.0f - flipP) * 255);
        if (oldAlpha > 20) {
            drawCalendarPage(gfx, pageCx, oldY, "FEB", colors.eye, oldAlpha);
        }
        int16_t newY = (int16_t)lerp((float)(pageCy + 30), (float)pageCy, flipP);
        uint8_t newAlpha = (uint8_t)(flipP * 255);
        drawCalendarPage(gfx, pageCx, newY, "MAR", colors.eye, newAlpha);
        drawDateGrid(gfx, pageCx, newY, -1, 0.0f, colors.eye, colors.bg, newAlpha);
    } else if (phaseIs(ph.name, "flip3")) {
        // Page flip MAR -> APR (target month)
        float flipP = ease::inOut(ph.p);
        int16_t oldY = (int16_t)((float)pageCy - flipP * 30.0f);
        uint8_t oldAlpha = (uint8_t)((1.0f - flipP) * 255);
        if (oldAlpha > 20) {
            drawCalendarPage(gfx, pageCx, oldY, "MAR", colors.eye, oldAlpha);
        }
        int16_t newY = (int16_t)lerp((float)(pageCy + 30), (float)pageCy, flipP);
        uint8_t newAlpha = (uint8_t)(flipP * 255);
        drawCalendarPage(gfx, pageCx, newY, "APR", colors.eye, newAlpha);
        drawDateGrid(gfx, pageCx, newY, -1, 0.0f, colors.eye, colors.bg, newAlpha);
    } else if (phaseIs(ph.name, "land")) {
        // Settled on APR page
        drawCalendarPage(gfx, pageCx, pageCy, "APR", colors.eye, 255);
        drawDateGrid(gfx, pageCx, pageCy, -1, 0.0f, colors.eye, colors.bg, 255);
    } else if (phaseIs(ph.name, "highlight")) {
        // Date "22" highlights with growing glow
        drawCalendarPage(gfx, pageCx, pageCy, "APR", colors.eye, 255);
        float glowP = ease::out(ph.p);
        drawDateGrid(gfx, pageCx, pageCy, 22, glowP, colors.eye, colors.bg, 255);
        // "22" text near highlighted position
        gfx.drawText("22", (int16_t)(pageCx + 26), (int16_t)(pageCy + 34), colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, (uint8_t)(glowP * 255));
    } else if (phaseIs(ph.name, "ping")) {
        // Calendar still visible + bell with expanding rings
        drawCalendarPage(gfx, pageCx, pageCy, "APR", colors.eye, 200);
        drawDateGrid(gfx, pageCx, pageCy, 22, 1.0f, colors.eye, colors.bg, 200);

        // Bell on right side
        int16_t bellX = SCX + 70, bellY = CY - 10;
        float shake = sinf(ph.p * TAU * 6.0f) * (3.0f + ph.p * 4.0f);
        drawReminderBell(gfx, bellX, bellY, shake, colors.eye, 255);

        // Expanding rings
        for (int i = 0; i < 3; i++) {
            float rp = fmodf(ph.p * 2.0f + (float)i * 0.33f, 1.0f);
            int16_t rr = (int16_t)(14.0f + rp * 24.0f);
            uint8_t op = (uint8_t)((1.0f - rp) * 150);
            gfx.drawArc(bellX, bellY, rr, 0, TAU, colors.eye, 2, op);
        }
    } else if (phaseIs(ph.name, "tear")) {
        // Page tears off upward
        float ep = ease::in(ph.p);
        int16_t tearY = (int16_t)lerp((float)pageCy, (float)(STATUS_H - 60), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        // Slight rotation as it tears
        float tearRot = ep * 12.0f;
        gfx.pushTransform();
        gfx.translate((float)pageCx, (float)tearY);
        gfx.rotate(tearRot, 0, 0);
        gfx.translate(-(float)pageCx, -(float)tearY);
        drawCalendarPage(gfx, pageCx, tearY, "APR", colors.eye, alpha);
        gfx.popTransform();
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "flip1", "flip2", "flip3")) {
        drawActLabel(gfx, "FLIPPING", colors.eye);
    } else if (phaseIs(ph.name, "highlight")) {
        drawActLabel(gfx, "MARK DATE", colors.eye);
    } else if (phaseIs(ph.name, "ping")) {
        drawActLabel(gfx, "REMINDER", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ALL SET", colors.eye);
    }
}

const VariantDef CALENDAR_VARIANTS[] = {
    {"calendar-mark", "Calendar Mark", 14000, TONE_NONE, render_calendar},
};

extern const CategoryDef CAT_CALENDAR = {
    "calendar", "Calendar", ContentKind::SCENE, TONE_CYAN,
    CALENDAR_VARIANTS, sizeof(CALENDAR_VARIANTS) / sizeof(CALENDAR_VARIANTS[0]), false
};
