#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry PLANT_PHASES[] = {
    {"waiting", 0.06f},   // Luni curious, centered
    {"potIn",   0.07f},   // Flower pot slides up from bottom
    {"shrink",  0.05f},   // Eyes shrink to top-right corner
    {"seed",    0.06f},   // Seed drops into pot
    {"water",   0.10f},   // Watering can pours
    {"sprout",  0.10f},   // Stem sprouts upward
    {"leaves",  0.10f},   // Leaves appear on stem
    {"bud",     0.10f},   // Bud forms at top
    {"bloom",   0.12f},   // Flower blooms (petals open)
    {"admire",  0.08f},   // Admire the flower
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.09f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(PLANT_PHASES) / sizeof(PLANT_PHASES[0]);

/* -- Helper: draw flower pot (trapezoid) --------------------------------- */
static void drawFlowerPot(GfxEngine& gfx, int16_t cx, int16_t topY,
                          uint16_t color, uint8_t alpha) {
    // Pot rim (wider bar at top)
    gfx.fillRect((int16_t)(cx - 30), topY, 60, 8, color, alpha);
    // Pot body (trapezoid: two triangles)
    int16_t botY = topY + 32;
    gfx.fillTriangle((int16_t)(cx - 26), (int16_t)(topY + 8),
                     (int16_t)(cx + 26), (int16_t)(topY + 8),
                     (int16_t)(cx + 20), botY, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 26), (int16_t)(topY + 8),
                     (int16_t)(cx + 20), botY,
                     (int16_t)(cx - 20), botY, color, alpha);
}

/* -- Helper: draw watering can ------------------------------------------- */
static void drawWateringCan(GfxEngine& gfx, int16_t cx, int16_t cy,
                            float tilt, uint16_t color, uint8_t alpha) {
    gfx.pushTransform();
    gfx.rotate(tilt, (float)cx, (float)cy);

    // Body
    gfx.fillRoundedRect((int16_t)(cx - 20), (int16_t)(cy - 12),
                        40, 24, 4, color, alpha);
    // Handle
    gfx.fillRect((int16_t)(cx - 30), (int16_t)(cy - 4),
                 10, 6, color, alpha);
    gfx.fillRoundedRect((int16_t)(cx - 36), (int16_t)(cy - 12),
                        6, 18, 2, color, alpha);
    // Spout
    gfx.drawLine((int16_t)(cx + 20), (int16_t)(cy - 12),
                 (int16_t)(cx + 32), (int16_t)(cy - 24), color, 3, alpha);

    gfx.popTransform();
}

/* -- Main render --------------------------------------------------------- */
static void render_plant_grow(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, PLANT_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "waiting")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "potIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "seed", "water")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIn(ph.name, "sprout", "leaves")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "bud")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIn(ph.name, "bloom", "admire")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "love";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "happy";
    }

    /* --- Props -------------------------------------------------------- */
    int16_t potCx = SCX;
    int16_t potTopY = CY + 30;   // top of pot rim
    int16_t soilY   = potTopY + 8;  // soil line (inside pot top)
    int16_t stemBaseX = potCx;

    // Calculate how far the plant has grown (used in multiple phases)
    float stemHeight = 0.0f;   // pixels of stem grown
    float leafReveal = 0.0f;   // 0..1 leaf visibility
    float budReveal  = 0.0f;   // 0..1 bud visibility
    float bloomReveal = 0.0f;  // 0..1 bloom (petals open)
    bool  showSeed    = false;
    float seedY       = 0.0f;
    bool  showWater   = false;
    float waterTilt   = 0.0f;
    float waterDrops  = 0.0f;
    bool  showPot     = false;
    uint8_t potAlpha  = 255;
    int16_t potDrawY  = potTopY;

    if (phaseIs(ph.name, "potIn")) {
        showPot = true;
        float ep = ease::out(ph.p);
        potDrawY = (int16_t)lerp((float)(SCREEN_H + 30), (float)potTopY, ep);
        potAlpha = (uint8_t)(ep * 255);
    } else if (phaseIs(ph.name, "shrink")) {
        showPot = true;
    } else if (phaseIs(ph.name, "seed")) {
        showPot = true;
        showSeed = true;
        // Seed drops from above into pot
        float ep = ease::out(ph.p);
        seedY = lerp((float)(potTopY - 60), (float)soilY, ep);
    } else if (phaseIs(ph.name, "water")) {
        showPot = true;
        showWater = true;
        // Watering can tilts and pours
        float tiltP = clamp(ph.p * 2.0f, 0.0f, 1.0f);
        waterTilt = tiltP * 30.0f;
        waterDrops = ph.p;
    } else if (phaseIs(ph.name, "sprout")) {
        showPot = true;
        stemHeight = ease::out(ph.p) * 70.0f;
    } else if (phaseIs(ph.name, "leaves")) {
        showPot = true;
        stemHeight = 70.0f;
        leafReveal = ease::out(ph.p);
    } else if (phaseIs(ph.name, "bud")) {
        showPot = true;
        stemHeight = 70.0f;
        leafReveal = 1.0f;
        budReveal = ease::out(ph.p);
    } else if (phaseIs(ph.name, "bloom")) {
        showPot = true;
        stemHeight = 70.0f;
        leafReveal = 1.0f;
        budReveal = 1.0f;
        bloomReveal = ease::out(ph.p);
    } else if (phaseIs(ph.name, "admire")) {
        showPot = true;
        stemHeight = 70.0f;
        leafReveal = 1.0f;
        budReveal = 1.0f;
        bloomReveal = 1.0f;
    } else if (phaseIs(ph.name, "grow")) {
        // Plant still visible but fading as eyes return
        showPot = true;
        stemHeight = 70.0f;
        leafReveal = 1.0f;
        budReveal = 1.0f;
        bloomReveal = 1.0f;
        potAlpha = (uint8_t)((1.0f - ease::in(ph.p)) * 255);
    }

    // Draw pot
    if (showPot) {
        drawFlowerPot(gfx, potCx, potDrawY, colors.eye, potAlpha);
    }

    // Draw seed
    if (showSeed) {
        gfx.fillCircle(stemBaseX, (int16_t)seedY, 4, colors.eye);
    }

    // Draw watering can and drops
    if (showWater) {
        int16_t canX = potCx + 50;
        int16_t canY = potTopY - 40;
        drawWateringCan(gfx, canX, canY, waterTilt, colors.eye, 255);

        // Water drops
        if (waterDrops > 0.3f) {
            for (int i = 0; i < 4; i++) {
                float dp = fmodf((waterDrops - 0.3f) * 3.0f + (float)i * 0.2f, 1.0f);
                int16_t dx = (int16_t)(canX + 24 + i * 3);
                int16_t dy = (int16_t)((float)canY - 16.0f + dp * 60.0f);
                uint8_t dAlpha = (uint8_t)((1.0f - dp) * 220);
                gfx.fillEllipse(dx, dy, 2, 4, colors.eye, dAlpha);
            }
        }
    }

    // Draw stem
    if (stemHeight > 1.0f && showPot) {
        int16_t stemTop = (int16_t)((float)soilY - stemHeight);
        gfx.drawLine(stemBaseX, soilY, stemBaseX, stemTop,
                     colors.eye, 4, potAlpha);

        // Leaves (two, on alternate sides)
        if (leafReveal > 0.01f) {
            uint8_t leafAlpha = (uint8_t)(leafReveal * (float)potAlpha);
            int16_t leafY1 = (int16_t)((float)stemTop + stemHeight * 0.55f);
            int16_t leafY2 = (int16_t)((float)stemTop + stemHeight * 0.30f);

            gfx.pushTransform();
            gfx.rotate(-30.0f, (float)(stemBaseX - 16), (float)leafY1);
            gfx.fillEllipse((int16_t)(stemBaseX - 16), leafY1,
                            12, 6, colors.eye, leafAlpha);
            gfx.popTransform();

            gfx.pushTransform();
            gfx.rotate(30.0f, (float)(stemBaseX + 16), (float)leafY2);
            gfx.fillEllipse((int16_t)(stemBaseX + 16), leafY2,
                            12, 6, colors.eye, leafAlpha);
            gfx.popTransform();
        }

        // Bud (small circle at top)
        if (budReveal > 0.01f) {
            uint8_t budAlpha = (uint8_t)(budReveal * (float)potAlpha);
            int16_t budR = (int16_t)(6.0f * budReveal);
            gfx.fillCircle(stemBaseX, stemTop, budR, colors.eye, budAlpha);
        }

        // Flower bloom (petals around center)
        if (bloomReveal > 0.01f) {
            uint8_t bAlpha = (uint8_t)(bloomReveal * (float)potAlpha);
            int16_t petalDist = (int16_t)(12.0f * bloomReveal);

            for (int i = 0; i < 6; i++) {
                float a = (float)i / 6.0f * TAU;
                int16_t px = (int16_t)((float)stemBaseX + cosf(a) * (float)petalDist);
                int16_t py = (int16_t)((float)stemTop + sinf(a) * (float)petalDist);

                gfx.pushTransform();
                gfx.rotate((float)i / 6.0f * 360.0f, (float)px, (float)py);
                gfx.fillEllipse(px, py,
                                (int16_t)(9.0f * bloomReveal),
                                (int16_t)(5.0f * bloomReveal),
                                colors.eye, bAlpha);
                gfx.popTransform();
            }

            // Center of flower (background color to create contrast)
            gfx.fillCircle(stemBaseX, stemTop,
                           (int16_t)(5.0f * bloomReveal), colors.bg, bAlpha);
        }
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   colors.eye, colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "seed", "water")) {
        drawActLabel(gfx, "PLANTING", colors.eye);
    } else if (phaseIn(ph.name, "sprout", "leaves", "bud")) {
        drawActLabel(gfx, "GROWING", colors.eye);
    } else if (phaseIn(ph.name, "bloom", "admire")) {
        drawActLabel(gfx, "BLOOMING", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "BEAUTIFUL", colors.eye);
    }
}

const VariantDef PLANT_VARIANTS[] = {
    {"plant-grow", "Plant Grow", 14000, TONE_NONE, render_plant_grow},
};

extern const CategoryDef CAT_PLANT = {
    "plant", "Plant", ContentKind::SCENE, TONE_GREEN,
    PLANT_VARIANTS, sizeof(PLANT_VARIANTS) / sizeof(PLANT_VARIANTS[0]), false
};
