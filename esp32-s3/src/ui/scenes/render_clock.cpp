#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry CLOCK_PHASES[] = {
    {"idle",    0.06f},   // Luni steady, centered
    {"rimIn",   0.07f},   // Clock rim circle fades in
    {"ticks",   0.07f},   // 12 tick marks fade in around rim
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"handsIn", 0.06f},   // Hour + minute hands appear
    {"sweep",   0.28f},   // Hands sweep through hours (rotation)
    {"chime",   0.10f},   // Bell on top with sound wave arcs
    {"settle",  0.08f},   // Settle down, bell calms
    {"rimOut",  0.07f},   // Rim exits (fades out)
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.09f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(CLOCK_PHASES) / sizeof(CLOCK_PHASES[0]);

/* -- Helper: draw clock rim + ticks -------------------------------------- */
static void drawClockRim(GfxEngine& gfx, int16_t cx, int16_t cy, int16_t r,
                         uint16_t color, uint8_t rimAlpha, float tickReveal) {
    gfx.pushAlpha(rimAlpha);
    gfx.strokeCircle(cx, cy, r, color, 3);

    int tickCount = (int)(tickReveal * 12.0f);
    for (int i = 0; i < tickCount; i++) {
        float a = (float)i / 12.0f * TAU - PI / 2.0f;
        int16_t inner = (i % 3 == 0) ? r - 14 : r - 8;
        gfx.drawLine((int16_t)(cx + cosf(a) * inner), (int16_t)(cy + sinf(a) * inner),
                     (int16_t)(cx + cosf(a) * (r - 2)), (int16_t)(cy + sinf(a) * (r - 2)),
                     color, (i % 3 == 0) ? 3 : 2);
    }
    gfx.popAlpha();
}

/* -- Helper: draw clock hands -------------------------------------------- */
static void drawClockHands(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float hourAngle, float minAngle,
                           uint16_t color, uint8_t alpha) {
    // Hour hand (short, thick)
    float hA = hourAngle - PI / 2.0f;
    gfx.drawLine(cx, cy,
                 (int16_t)(cx + cosf(hA) * 34),
                 (int16_t)(cy + sinf(hA) * 34),
                 color, 6, alpha);

    // Minute hand (long, thinner)
    float mA = minAngle - PI / 2.0f;
    gfx.drawLine(cx, cy,
                 (int16_t)(cx + cosf(mA) * 54),
                 (int16_t)(cy + sinf(mA) * 54),
                 color, 4, alpha);

    // Center dot
    gfx.fillCircle(cx, cy, 4, color, alpha);
}

/* -- Helper: draw chime bell --------------------------------------------- */
static void drawChimeBell(GfxEngine& gfx, int16_t cx, int16_t bellTop,
                          float shake, uint16_t color, uint8_t alpha) {
    int16_t sx = (int16_t)shake;
    // Small bell dome
    gfx.fillCircle((int16_t)(cx + sx), bellTop, 10, color, alpha);
    // Bell body
    gfx.fillRoundedRect((int16_t)(cx - 10 + sx), (int16_t)(bellTop + 6),
                        20, 14, 3, color, alpha);
    // Clapper
    gfx.fillCircle((int16_t)(cx + sx), (int16_t)(bellTop + 22), 3, color, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_clock_tower(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, CLOCK_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIn(ph.name, "rimIn", "ticks")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "handsIn", "sweep")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIn(ph.name, "chime", "settle")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "rimOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Clock props -------------------------------------------------- */
    int16_t clockCx = SCX, clockCy = CY + 4;
    int16_t radius = 70;

    if (phaseIs(ph.name, "rimIn")) {
        // Rim fades in
        uint8_t alpha = (uint8_t)(ease::out(ph.p) * 255);
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, alpha, 0.0f);
    } else if (phaseIs(ph.name, "ticks")) {
        // Tick marks reveal one by one
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, ph.p);
    } else if (phaseIs(ph.name, "shrink")) {
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, 1.0f);
    } else if (phaseIs(ph.name, "handsIn")) {
        // Full rim + ticks, hands fade in
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, 1.0f);
        uint8_t hAlpha = (uint8_t)(ease::out(ph.p) * 255);
        drawClockHands(gfx, clockCx, clockCy,
                       0.0f, 0.0f, colors.eye, hAlpha);
    } else if (phaseIs(ph.name, "sweep")) {
        // Hands sweep through hours
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, 1.0f);
        float sweepP = ease::inOut(ph.p);
        float hourAngle = sweepP * TAU * 2.0f;     // 2 full rotations of hour hand
        float minAngle  = sweepP * TAU * 24.0f;    // minute hand spins faster
        drawClockHands(gfx, clockCx, clockCy,
                       hourAngle, minAngle, colors.eye, 255);
    } else if (phaseIs(ph.name, "chime")) {
        // Clock at 12 o'clock, bell chimes on top
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, 1.0f);
        drawClockHands(gfx, clockCx, clockCy, 0.0f, 0.0f, colors.eye, 255);

        // Bell on top of clock
        float shake = sinf(ph.p * TAU * 8.0f) * 6.0f * (1.0f - ph.p * 0.5f);
        int16_t bellTop = clockCy - radius - 20;
        drawChimeBell(gfx, clockCx, bellTop, shake, colors.eye, 255);

        // Sound wave arcs
        for (int i = 0; i < 3; i++) {
            float waveP = fmodf(ph.p * 3.0f + (float)i * 0.33f, 1.0f);
            uint8_t wAlpha = (uint8_t)((1.0f - waveP) * 180);
            float wR = 14.0f + waveP * 24.0f;
            gfx.drawArc(clockCx, bellTop, (int16_t)wR,
                        PI + 0.4f, TAU - 0.4f, colors.eye, 2, wAlpha);
        }
    } else if (phaseIs(ph.name, "settle")) {
        // Calm down, bell fading, hands at 12
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, 255, 1.0f);
        drawClockHands(gfx, clockCx, clockCy, 0.0f, 0.0f, colors.eye, 255);

        float shake = sinf(ph.p * TAU * 4.0f) * 3.0f * (1.0f - ph.p);
        int16_t bellTop = clockCy - radius - 20;
        uint8_t bellAlpha = (uint8_t)((1.0f - ease::in(ph.p)) * 255);
        drawChimeBell(gfx, clockCx, bellTop, shake, colors.eye, bellAlpha);
    } else if (phaseIs(ph.name, "rimOut")) {
        // Everything fades out
        uint8_t alpha = (uint8_t)((1.0f - ease::in(ph.p)) * 255);
        drawClockRim(gfx, clockCx, clockCy, radius, colors.eye, alpha, 1.0f);
        drawClockHands(gfx, clockCx, clockCy, 0.0f, 0.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "handsIn", "sweep", "chime", "settle")) {
        drawActLabel(gfx, "CLOCK", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "TIME", colors.eye);
    }
}

const VariantDef CLOCK_VARIANTS[] = {
    {"clock-tower", "Clock Tower", 14000, TONE_NONE, render_clock_tower},
};

extern const CategoryDef CAT_CLOCK = {
    "clock", "Clock", ContentKind::SCENE, TONE_CYAN,
    CLOCK_VARIANTS, sizeof(CLOCK_VARIANTS) / sizeof(CLOCK_VARIANTS[0]), false
};
