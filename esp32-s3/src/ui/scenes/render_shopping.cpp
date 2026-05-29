#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry SHOP_PHASES[] = {
    {"idle",   0.06f},   // Luni eager, centered
    {"bagIn",  0.07f},   // Shopping bag drops in from top
    {"shrink", 0.05f},   // Eyes shrink to top-right corner
    {"item1",  0.12f},   // Circle item bounces into bag
    {"item2",  0.12f},   // Rect item bounces into bag
    {"item3",  0.12f},   // Triangle item bounces into bag
    {"tag",    0.10f},   // Price tag flashes
    {"bagOut", 0.07f},   // Bag exits downward
    {"grow",   0.07f},   // Eyes grow back to center
    {"done",   0.22f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(SHOP_PHASES) / sizeof(SHOP_PHASES[0]);

/* -- Helper: draw shopping bag ------------------------------------------- */
static void drawBag(GfxEngine& gfx, int16_t cx, int16_t cy,
                    uint16_t color, uint8_t alpha) {
    // Bag body (trapezoid: narrower top, wider bottom)
    int16_t topW = 52, botW = 66, h = 60;
    int16_t topY = (int16_t)(cy - h / 2);
    int16_t botY = (int16_t)(cy + h / 2);
    // Left side
    gfx.fillTriangle((int16_t)(cx - topW / 2), topY,
                     (int16_t)(cx - botW / 2), botY,
                     (int16_t)(cx + botW / 2), botY,
                     color, alpha);
    // Right side (completes the trapezoid)
    gfx.fillTriangle((int16_t)(cx - topW / 2), topY,
                     (int16_t)(cx + topW / 2), topY,
                     (int16_t)(cx + botW / 2), botY,
                     color, alpha);
    // Bottom edge
    gfx.fillRoundedRect((int16_t)(cx - botW / 2), (int16_t)(botY - 6),
                        botW, 6, 3, color, alpha);

    // Handle arcs
    gfx.drawArc((int16_t)(cx - 10), topY, 10,
                PI, TAU, color, 3, alpha);
    gfx.drawArc((int16_t)(cx + 10), topY, 10,
                PI, TAU, color, 3, alpha);
}

/* -- Helper: draw bouncing item into bag --------------------------------- */
static void drawItemBounce(GfxEngine& gfx, int16_t cx, int16_t cy,
                           int shape, float p, uint16_t color) {
    // Item arcs from above to land inside bag
    // Parabolic path: start above-left, arc to bag center
    float arcP = ease::out(clamp(p / 0.6f, 0.0f, 1.0f));
    int16_t startX = (int16_t)(cx - 80 + shape * 40);
    int16_t startY = (int16_t)(cy - 80);
    int16_t endX = (int16_t)(cx + (shape - 1) * 14);
    int16_t endY = (int16_t)(cy - 6);

    int16_t ix = (int16_t)lerp((float)startX, (float)endX, arcP);
    int16_t iy = (int16_t)lerp((float)startY, (float)endY, arcP);
    // Add arc height
    float arcHeight = sinf(arcP * PI) * 40.0f;
    iy = (int16_t)((float)iy - arcHeight);

    // Once landed, just show at rest position
    if (p > 0.6f) {
        ix = endX;
        iy = endY;
    }

    uint8_t op = (uint8_t)(clamp(p * 3.0f, 0.0f, 1.0f) * 255);

    if (shape == 0) {
        // Circle
        gfx.fillCircle(ix, iy, 8, color, op);
    } else if (shape == 1) {
        // Rectangle
        gfx.fillRoundedRect((int16_t)(ix - 7), (int16_t)(iy - 5), 14, 10, 2, color, op);
    } else {
        // Triangle
        gfx.fillTriangle((int16_t)(ix - 7), (int16_t)(iy + 6),
                         (int16_t)(ix + 7), (int16_t)(iy + 6),
                         ix, (int16_t)(iy - 7),
                         color, op);
    }
}

/* -- Helper: draw price tag ---------------------------------------------- */
static void drawPriceTag(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float flash, uint16_t color) {
    uint8_t alpha = (uint8_t)(180 + sinf(flash * TAU * 3.0f) * 75.0f);
    // Tag body
    gfx.fillRoundedRect((int16_t)(cx + 36), (int16_t)(cy - 16), 36, 22, 4, color, alpha);
    // Hole
    gfx.fillCircle((int16_t)(cx + 40), (int16_t)(cy - 5), 3, COLOR_BG, alpha);
    // "$" text
    gfx.drawText("$", (int16_t)(cx + 54), (int16_t)(cy + 2), COLOR_BG, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_shopping(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, SHOP_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "bagIn")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "item1", "item2", "item3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "tag")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "bagOut")) {
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

    /* --- Prop: shopping bag ------------------------------------------- */
    int16_t bagCx = SCX, bagCy = CY + 14;

    if (phaseIs(ph.name, "bagIn")) {
        // Drops in from top
        float ep = ease::out(ph.p);
        bagCy = (int16_t)lerp((float)(STATUS_H - 60), (float)(CY + 14), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawBag(gfx, bagCx, bagCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawBag(gfx, bagCx, bagCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "item1")) {
        drawBag(gfx, bagCx, bagCy, colors.eye, 255);
        drawItemBounce(gfx, bagCx, bagCy, 0, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "item2")) {
        drawBag(gfx, bagCx, bagCy, colors.eye, 255);
        // Item 1 at rest
        drawItemBounce(gfx, bagCx, bagCy, 0, 1.0f, colors.eye);
        drawItemBounce(gfx, bagCx, bagCy, 1, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "item3")) {
        drawBag(gfx, bagCx, bagCy, colors.eye, 255);
        // Items 1-2 at rest
        drawItemBounce(gfx, bagCx, bagCy, 0, 1.0f, colors.eye);
        drawItemBounce(gfx, bagCx, bagCy, 1, 1.0f, colors.eye);
        drawItemBounce(gfx, bagCx, bagCy, 2, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "tag")) {
        drawBag(gfx, bagCx, bagCy, colors.eye, 255);
        // All items at rest
        drawItemBounce(gfx, bagCx, bagCy, 0, 1.0f, colors.eye);
        drawItemBounce(gfx, bagCx, bagCy, 1, 1.0f, colors.eye);
        drawItemBounce(gfx, bagCx, bagCy, 2, 1.0f, colors.eye);
        // Price tag flashes
        drawPriceTag(gfx, bagCx, bagCy, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "bagOut")) {
        // Bag exits downward
        float ep = ease::in(ph.p);
        bagCy = (int16_t)lerp((float)(CY + 14), (float)(SCREEN_H + 60), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawBag(gfx, bagCx, bagCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_ORANGE], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "item1", "item2", "item3")) {
        drawActLabel(gfx, "SHOPPING", colors.eye);
    } else if (phaseIs(ph.name, "tag")) {
        drawActLabel(gfx, "CHECKOUT", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ALL DONE", colors.eye);
    }
}

const VariantDef SHOPPING_VARIANTS[] = {
    {"shopping-main", "Shopping", 14000, TONE_NONE, render_shopping},
};

extern const CategoryDef CAT_SHOPPING = {
    "shopping", "Shopping", ContentKind::SCENE, TONE_ORANGE,
    SHOPPING_VARIANTS, sizeof(SHOPPING_VARIANTS) / sizeof(SHOPPING_VARIANTS[0]), false
};
