#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry FLASH_PHASES[] = {
    {"dark",     0.07f},   // Dark/curious Luni, centered
    {"torchIn",  0.07f},   // Flashlight enters from left
    {"shrink",   0.05f},   // Eyes shrink to LEFT corner
    {"click",    0.05f},   // Click on — beam appears
    {"sweep1",   0.12f},   // Beam sweeps right
    {"reveal1",  0.07f},   // Reveals gem
    {"sweep2",   0.12f},   // Beam sweeps further
    {"reveal2",  0.07f},   // Reveals key
    {"sweep3",   0.12f},   // Beam sweeps further
    {"reveal3",  0.07f},   // Reveals star
    {"click2",   0.05f},   // Click off — beam fades
    {"torchOut", 0.05f},   // Torch exits left
    {"grow",     0.05f},   // Eyes grow back to center
    {"done",     0.04f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(FLASH_PHASES) / sizeof(FLASH_PHASES[0]);

/* ── Corner constants (LEFT side for this scene) ─────────────────── */
static constexpr float CORNER_X = 34.0f;
static constexpr float CORNER_Y = (float)(STATUS_H + 24);
static constexpr float CORNER_SCALE = 0.30f;

/* ── Helper: draw flashlight body ────────────────────────────────── */
static void drawFlashlight(GfxEngine& gfx, int16_t cx, int16_t cy,
                           uint16_t color, uint8_t alpha) {
    // Body (horizontal rect)
    gfx.fillRoundedRect((int16_t)(cx - 30), (int16_t)(cy - 8), 44, 16, 3,
                        color, alpha);
    // Head (cone shape as trapezoid, right side)
    gfx.fillTriangle((int16_t)(cx + 14), (int16_t)(cy - 8),
                     (int16_t)(cx + 14), (int16_t)(cy + 8),
                     (int16_t)(cx + 26), (int16_t)(cy + 12), color, alpha);
    gfx.fillTriangle((int16_t)(cx + 14), (int16_t)(cy - 8),
                     (int16_t)(cx + 26), (int16_t)(cy - 12),
                     (int16_t)(cx + 26), (int16_t)(cy + 12), color, alpha);
    // Lens ring
    gfx.fillRect((int16_t)(cx + 24), (int16_t)(cy - 13), 4, 26, color, alpha);
    // Button on top
    gfx.fillCircle((int16_t)(cx - 8), (int16_t)(cy - 10), 3, color, alpha);
}

/* ── Helper: draw beam cone ──────────────────────────────────────── */
static void drawBeam(GfxEngine& gfx, int16_t startX, int16_t cy,
                     float beamAngle, float beamLen,
                     uint16_t color) {
    // Beam as a cone with gradient alpha layers
    for (int layer = 3; layer >= 0; layer--) {
        float spread = (float)(layer + 1) * 0.08f;
        float layerLen = beamLen * (0.6f + (float)layer * 0.13f);
        uint8_t alpha = (uint8_t)(60 - layer * 12);

        int16_t endX = (int16_t)(startX + cosf(beamAngle) * layerLen);
        int16_t topY = (int16_t)(cy - sinf(spread) * layerLen);
        int16_t botY = (int16_t)(cy + sinf(spread) * layerLen);

        gfx.fillTriangle(startX, cy, endX, topY, endX, botY, color, alpha);
    }
    // Bright center line
    int16_t endX = (int16_t)(startX + cosf(beamAngle) * beamLen * 0.8f);
    gfx.drawLine(startX, cy, endX, cy, color, 2, 100);
}

/* ── Helper: draw gem (pentagon) ─────────────────────────────────── */
static void drawGem(GfxEngine& gfx, int16_t cx, int16_t cy,
                    float scale, uint16_t color, uint8_t alpha) {
    float r = 12.0f * scale;
    // Pentagon approximation using triangles
    for (int i = 0; i < 5; i++) {
        float a0 = -PI / 2.0f + (float)i * (TAU / 5.0f);
        float a1 = -PI / 2.0f + (float)(i + 1) * (TAU / 5.0f);
        int16_t x0 = (int16_t)(cx + cosf(a0) * r);
        int16_t y0 = (int16_t)(cy + sinf(a0) * r);
        int16_t x1 = (int16_t)(cx + cosf(a1) * r);
        int16_t y1 = (int16_t)(cy + sinf(a1) * r);
        gfx.fillTriangle(x0, y0, x1, y1, cx, cy, color, alpha);
    }
    // Inner sparkle
    gfx.fillCircle(cx, (int16_t)(cy - 2), (int16_t)(3.0f * scale),
                   COLOR_BG, (uint8_t)(alpha / 2));
}

/* ── Helper: draw key ────────────────────────────────────────────── */
static void drawKey(GfxEngine& gfx, int16_t cx, int16_t cy,
                    float scale, uint16_t color, uint8_t alpha) {
    int16_t headR = (int16_t)(8.0f * scale);
    // Key head (circle with hole)
    gfx.strokeCircle(cx, cy, headR, color, 2);
    gfx.fillCircle(cx, cy, (int16_t)(3.0f * scale), COLOR_BG, alpha);
    // Key shaft
    int16_t shaftLen = (int16_t)(20.0f * scale);
    gfx.fillRect((int16_t)(cx + headR - 2), (int16_t)(cy - 2),
                 shaftLen, 4, color, alpha);
    // Key teeth
    for (int i = 0; i < 3; i++) {
        int16_t tx = (int16_t)(cx + headR + 4 + i * 6 * scale);
        gfx.fillRect(tx, (int16_t)(cy + 2), (int16_t)(3.0f * scale),
                     (int16_t)(5.0f * scale), color, alpha);
    }
}

/* ── Helper: draw star ───────────────────────────────────────────── */
static void drawStar(GfxEngine& gfx, int16_t cx, int16_t cy,
                     float r1, float r2, uint16_t color, uint8_t alpha) {
    for (int i = 0; i < 5; i++) {
        float a0 = -PI / 2.0f + (float)i * (TAU / 5.0f);
        float a1 = a0 + TAU / 10.0f;
        float a2 = a0 + TAU / 5.0f;
        int16_t x0 = (int16_t)(cx + cosf(a0) * r1);
        int16_t y0 = (int16_t)(cy + sinf(a0) * r1);
        int16_t x1 = (int16_t)(cx + cosf(a1) * r2);
        int16_t y1 = (int16_t)(cy + sinf(a1) * r2);
        int16_t x2 = (int16_t)(cx + cosf(a2) * r1);
        int16_t y2 = (int16_t)(cy + sinf(a2) * r1);
        gfx.fillTriangle(x0, y0, x1, y1, cx, cy, color, alpha);
        gfx.fillTriangle(x1, y1, x2, y2, cx, cy, color, alpha);
    }
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_flashlight(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, FLASH_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "dark")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "torchIn")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, CORNER_X, ep);
        eyeCy    = lerp((float)CY,  CORNER_Y, ep);
        eyeScale = lerp(1.0f, CORNER_SCALE, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "click", "sweep1", "sweep2", "sweep3")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "excited";
    } else if (phaseIn(ph.name, "reveal1", "reveal2", "reveal3")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "click2")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "torchOut")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(CORNER_X, (float)SCX, ep);
        eyeCy    = lerp(CORNER_Y, (float)CY,  ep);
        eyeScale = lerp(CORNER_SCALE, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Hidden items (drawn dim until revealed) ------------------ */
    /* Items at fixed positions on the right side of screen */
    int16_t item1x = 140, item1y = CY + 20;
    int16_t item2x = 210, item2y = CY - 10;
    int16_t item3x = 270, item3y = CY + 30;

    // Track cumulative reveal state
    bool gem_revealed = phaseIn(ph.name, "reveal1", "sweep2", "reveal2",
                                "sweep3") ||
                        phaseIn(ph.name, "reveal3", "click2", "torchOut");
    bool key_revealed = phaseIn(ph.name, "reveal2", "sweep3", "reveal3") ||
                        phaseIn(ph.name, "click2", "torchOut");
    bool star_revealed = phaseIn(ph.name, "reveal3", "click2", "torchOut");

    // Draw items dim or bright
    if (phaseIn(ph.name, "click", "sweep1") ||
        phaseIn(ph.name, "reveal1", "sweep2", "reveal2") ||
        phaseIn(ph.name, "sweep3", "reveal3") ||
        phaseIn(ph.name, "click2", "torchOut")) {
        uint8_t gemOp  = gem_revealed  ? (uint8_t)220 : (uint8_t)18;
        uint8_t keyOp  = key_revealed  ? (uint8_t)220 : (uint8_t)18;
        uint8_t starOp = star_revealed ? (uint8_t)220 : (uint8_t)18;

        drawGem(gfx, item1x, item1y, 1.0f, colors.eye, gemOp);
        drawKey(gfx, item2x, item2y, 1.0f, colors.eye, keyOp);
        drawStar(gfx, item3x, item3y, 14.0f, 6.0f, colors.eye, starOp);
    }

    /* --- Flashlight and beam -------------------------------------- */
    int16_t torchX = 80, torchY = CY + 6;
    bool beamOn = false;
    float beamAngle = 0.0f;
    float beamLen = 200.0f;

    if (phaseIs(ph.name, "torchIn")) {
        float ep = ease::out(ph.p);
        int16_t fromX = -50;
        torchX = (int16_t)lerp((float)fromX, 80.0f, ep);
        drawFlashlight(gfx, torchX, torchY, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
    } else if (phaseIs(ph.name, "click")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        // Beam flickers on
        float flickerOp = ease::out(ph.p);
        beamLen = 200.0f * flickerOp;
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, 0.0f, beamLen, colors.eye);
    } else if (phaseIs(ph.name, "sweep1")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        float sweepP = ease::inOut(ph.p);
        beamAngle = lerp(-0.15f, 0.05f, sweepP);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, beamAngle, 200.0f, colors.eye);
    } else if (phaseIs(ph.name, "reveal1")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, 0.05f, 200.0f, colors.eye);
        // Flash on gem
        float flash = sinf(ph.p * PI) * 60.0f;
        gfx.fillCircle(item1x, item1y, (int16_t)(16 + flash * 0.2f),
                       colors.eye, (uint8_t)flash);
    } else if (phaseIs(ph.name, "sweep2")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        float sweepP = ease::inOut(ph.p);
        beamAngle = lerp(0.05f, -0.12f, sweepP);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, beamAngle, 200.0f, colors.eye);
    } else if (phaseIs(ph.name, "reveal2")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, -0.12f, 200.0f, colors.eye);
        float flash = sinf(ph.p * PI) * 60.0f;
        gfx.fillCircle(item2x, item2y, (int16_t)(16 + flash * 0.2f),
                       colors.eye, (uint8_t)flash);
    } else if (phaseIs(ph.name, "sweep3")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        float sweepP = ease::inOut(ph.p);
        beamAngle = lerp(-0.12f, -0.22f, sweepP);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, beamAngle, 200.0f, colors.eye);
    } else if (phaseIs(ph.name, "reveal3")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        drawBeam(gfx, (int16_t)(torchX + 28), torchY, -0.22f, 200.0f, colors.eye);
        float flash = sinf(ph.p * PI) * 60.0f;
        gfx.fillCircle(item3x, item3y, (int16_t)(16 + flash * 0.2f),
                       colors.eye, (uint8_t)flash);
    } else if (phaseIs(ph.name, "click2")) {
        drawFlashlight(gfx, torchX, torchY, colors.eye, 255);
        float fadeP = ease::in(ph.p);
        beamLen = 200.0f * (1.0f - fadeP);
        if (beamLen > 5.0f) {
            drawBeam(gfx, (int16_t)(torchX + 28), torchY, -0.22f, beamLen,
                     colors.eye);
        }
    } else if (phaseIs(ph.name, "torchOut")) {
        float ep = ease::in(ph.p);
        torchX = (int16_t)lerp(80.0f, -60.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawFlashlight(gfx, torchX, torchY, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "sweep1", "reveal1", "sweep2", "reveal2")) {
        drawActLabel(gfx, "SEARCHING", colors.eye);
    } else if (phaseIn(ph.name, "sweep3", "reveal3")) {
        drawActLabel(gfx, "SEARCHING", colors.eye);
    } else if (phaseIn(ph.name, "click2", "torchOut")) {
        drawActLabel(gfx, "FOUND ALL", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "EXPLORER", colors.eye);
    }
}

const VariantDef FLASHLIGHT_VARIANTS[] = {
    {"flashlight-sweep", "Sweep & reveal", 14000, TONE_NONE, render_flashlight},
};

extern const CategoryDef CAT_FLASHLIGHT = {
    "flashlight", "Flashlight", ContentKind::SCENE, TONE_WARM,
    FLASHLIGHT_VARIANTS, sizeof(FLASHLIGHT_VARIANTS) / sizeof(FLASHLIGHT_VARIANTS[0]),
    false
};
