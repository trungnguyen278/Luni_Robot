#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry NIGHT_PHASES[] = {
    {"day",    0.08f},   // Luni steady, bright
    {"skyDim", 0.08f},   // Sky dims (no visual change in monochrome, emotion shifts)
    {"shrink", 0.06f},   // Eyes shrink to top-right corner
    {"sunset", 0.16f},   // Sun crosses and sets (circle descends)
    {"moon",   0.12f},   // Moon rises in corner (crescent)
    {"stars",  0.14f},   // Stars twinkle, appear one by one
    {"hoot",   0.10f},   // Owl "hoo" text
    {"grow",   0.08f},   // Eyes grow back to center
    {"done",   0.18f},   // Sleepy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(NIGHT_PHASES) / sizeof(NIGHT_PHASES[0]);

/* -- Star positions (pseudo-random scatter) ------------------------------ */
struct StarDef { int16_t x, y; int16_t r; float phase; };
static const StarDef STARS[] = {
    { 40,  45, 2, 0.0f},
    { 80,  65, 3, 0.2f},
    {130,  38, 2, 0.4f},
    {200,  55, 3, 0.1f},
    {250,  42, 2, 0.6f},
    {290,  70, 2, 0.3f},
    { 55, 100, 3, 0.5f},
    {175,  80, 2, 0.7f},
    {110, 110, 2, 0.15f},
    {240, 100, 3, 0.45f},
    { 30, 140, 2, 0.8f},
    {300, 130, 2, 0.35f},
};
static constexpr int STAR_COUNT = 12;

/* -- Helper: draw crescent moon ------------------------------------------ */
static void drawCrescent(GfxEngine& gfx, int16_t cx, int16_t cy,
                         int16_t r, uint16_t color, uint16_t bg,
                         uint8_t alpha) {
    gfx.fillCircle(cx, cy, r, color, alpha);
    // Cut out a smaller overlapping circle to make crescent
    gfx.fillCircle((int16_t)(cx + r / 2), (int16_t)(cy - r / 4),
                   (int16_t)(r - 3), bg, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_night_fall(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, NIGHT_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "day")) {
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "skyDim")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIs(ph.name, "sunset")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "moon")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "stars")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "hoot")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "sleepy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "sleepy";
    }

    /* --- Scene props -------------------------------------------------- */
    // Sun: descends during sunset phase
    if (phaseIs(ph.name, "sunset")) {
        float ep = ease::inOut(ph.p);
        // Sun moves from upper-left area down past bottom
        float sunX = lerp(60.0f, 140.0f, ep);
        float sunY = lerp(50.0f, (float)(SCREEN_H + 20), ep);
        uint8_t sunAlpha = (uint8_t)((1.0f - ep) * 255);
        int16_t sunR = 20;
        gfx.fillCircle((int16_t)sunX, (int16_t)sunY, sunR, colors.eye, sunAlpha);

        // Sun rays (fading out)
        for (int i = 0; i < 8; i++) {
            float a = (float)i / 8.0f * TAU;
            float inner = (float)sunR + 4.0f;
            float outer = (float)sunR + 12.0f;
            uint8_t rayAlpha = (uint8_t)((1.0f - ep) * 160);
            gfx.drawLine((int16_t)(sunX + cosf(a) * inner),
                         (int16_t)(sunY + sinf(a) * inner),
                         (int16_t)(sunX + cosf(a) * outer),
                         (int16_t)(sunY + sinf(a) * outer),
                         colors.eye, 2, rayAlpha);
        }
    }

    // Moon: rises during moon phase and stays
    bool showMoon = false;
    float moonAlpha = 0.0f;
    int16_t moonX = 50, moonY = 60;
    int16_t moonR = 18;

    if (phaseIs(ph.name, "moon")) {
        showMoon = true;
        float ep = ease::out(ph.p);
        moonY = (int16_t)lerp(160.0f, 60.0f, ep);
        moonAlpha = ep;
    } else if (phaseIn(ph.name, "stars", "hoot")) {
        showMoon = true;
        moonAlpha = 1.0f;
    }

    if (showMoon) {
        drawCrescent(gfx, moonX, moonY, moonR, colors.eye, colors.bg,
                     (uint8_t)(moonAlpha * 255));
    }

    // Stars: appear one by one during stars phase, stay through hoot
    int visibleStars = 0;
    float starTwinkleBase = 0.0f;
    if (phaseIs(ph.name, "stars")) {
        visibleStars = (int)(ph.p * (float)STAR_COUNT);
        starTwinkleBase = t;
    } else if (phaseIs(ph.name, "hoot")) {
        visibleStars = STAR_COUNT;
        starTwinkleBase = t;
    }

    for (int i = 0; i < visibleStars; i++) {
        float tw = 0.5f + fabsf(sinf((starTwinkleBase + STARS[i].phase) * TAU)) * 0.5f;
        uint8_t sAlpha = (uint8_t)(tw * 255);
        gfx.fillCircle(STARS[i].x, STARS[i].y, STARS[i].r, colors.eye, sAlpha);
    }

    // Hoot: owl "hoo" text
    if (phaseIs(ph.name, "hoot")) {
        // "hoo" text pulses
        float pulse1 = sinf(ph.p * TAU * 2.0f);
        uint8_t hooAlpha = (uint8_t)(150.0f + pulse1 * 80.0f);
        gfx.drawText("hoo..hoo", SCX, CY + 40, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, hooAlpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   colors.eye, colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "sunset", "moon")) {
        drawActLabel(gfx, "NIGHTFALL", colors.eye);
    } else if (phaseIn(ph.name, "stars", "hoot")) {
        drawActLabel(gfx, "GOOD NIGHT", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "GOOD NIGHT", colors.eye);
    }
}

const VariantDef NIGHT_VARIANTS[] = {
    {"night-fall", "Night Fall", 14000, TONE_NONE, render_night_fall},
};

extern const CategoryDef CAT_NIGHT = {
    "night", "Night", ContentKind::SCENE, TONE_PURPLE,
    NIGHT_VARIANTS, sizeof(NIGHT_VARIANTS) / sizeof(NIGHT_VARIANTS[0]), false
};
