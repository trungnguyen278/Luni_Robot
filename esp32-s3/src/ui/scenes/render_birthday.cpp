#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry BDAY_PHASES[] = {
    {"idle",     0.06f},   // Luni curious, centered
    {"cakeIn",   0.07f},   // Birthday cake rises from bottom
    {"shrink",   0.05f},   // Eyes shrink to top-right corner
    {"candles",  0.14f},   // Candles flicker with small flames
    {"blow",     0.10f},   // Blow out: flames fade, puff cloud
    {"confetti", 0.18f},   // Confetti burst, small rects/circles
    {"cakeOut",  0.07f},   // Cake exits downward
    {"grow",     0.07f},   // Eyes grow back to center
    {"done",     0.12f},   // "HAPPY BDAY" text
    {"wish",     0.14f},   // Hold final pose
};
static constexpr uint8_t PHASE_COUNT = sizeof(BDAY_PHASES) / sizeof(BDAY_PHASES[0]);

/* -- Helper: draw cake --------------------------------------------------- */
static void drawCake(GfxEngine& gfx, int16_t cx, int16_t cy,
                     uint16_t color, uint8_t alpha) {
    // Bottom layer (80x30)
    gfx.fillRoundedRect((int16_t)(cx - 40), (int16_t)(cy - 2), 80, 30, 4, color, alpha);
    // Top layer (60x24)
    gfx.fillRoundedRect((int16_t)(cx - 30), (int16_t)(cy - 24), 60, 24, 4, color, alpha);
    // Frosting line between layers
    gfx.drawLine((int16_t)(cx - 38), (int16_t)(cy - 1),
                 (int16_t)(cx + 38), (int16_t)(cy - 1), color, 2, alpha);
}

/* -- Helper: draw candle sticks ------------------------------------------ */
static void drawCandles(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint8_t alpha) {
    // 3 candles evenly spaced on top layer
    for (int i = -1; i <= 1; i++) {
        int16_t candleX = (int16_t)(cx + i * 18);
        int16_t candleTop = (int16_t)(cy - 24 - 16);
        // Candle stick (thin rect)
        gfx.fillRect((int16_t)(candleX - 2), candleTop, 4, 16, color, alpha);
    }
}

/* -- Helper: draw flames (triangles on top of candles) ------------------- */
static void drawFlames(GfxEngine& gfx, int16_t cx, int16_t cy,
                       float flicker, uint16_t color, uint8_t alpha) {
    for (int i = -1; i <= 1; i++) {
        int16_t fx = (int16_t)(cx + i * 18);
        int16_t flameBase = (int16_t)(cy - 24 - 16);
        float fOff = sinf(flicker * TAU * 4.0f + (float)i * 1.5f) * 3.0f;
        gfx.fillTriangle((int16_t)(fx - 4), flameBase,
                         (int16_t)(fx + 4), flameBase,
                         (int16_t)(fx + (int16_t)fOff), (int16_t)(flameBase - 12),
                         color, alpha);
    }
}

/* -- Helper: draw puff cloud --------------------------------------------- */
static void drawPuffCloud(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float progress, uint16_t color) {
    float r = 8.0f + progress * 20.0f;
    uint8_t alpha = (uint8_t)((1.0f - progress) * 180);
    gfx.fillCircle(cx, (int16_t)(cy - 44 - progress * 10.0f),
                   (int16_t)r, color, alpha);
    gfx.fillCircle((int16_t)(cx - 12), (int16_t)(cy - 40 - progress * 8.0f),
                   (int16_t)(r * 0.7f), color, alpha);
    gfx.fillCircle((int16_t)(cx + 12), (int16_t)(cy - 40 - progress * 8.0f),
                   (int16_t)(r * 0.7f), color, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_birthday(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, BDAY_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "cakeIn")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "surprised";
    } else if (phaseIn(ph.name, "candles", "blow")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "confetti")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "cakeOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIn(ph.name, "done", "wish")) {
        eyeEmo = "happy";
    }

    /* --- Prop: birthday cake ------------------------------------------ */
    int16_t cakeCx = SCX, cakeCy = CY + 20;

    if (phaseIs(ph.name, "cakeIn")) {
        // Rise from bottom
        float ep = ease::out(ph.p);
        cakeCy = (int16_t)lerp((float)(SCREEN_H + 40), (float)(CY + 20), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawCake(gfx, cakeCx, cakeCy, colors.eye, alpha);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawCake(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawFlames(gfx, cakeCx, cakeCy, ph.p, colors.eye, 255);
    } else if (phaseIs(ph.name, "candles")) {
        // Candles flickering
        drawCake(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawFlames(gfx, cakeCx, cakeCy, ph.p, colors.eye, 255);
    } else if (phaseIs(ph.name, "blow")) {
        drawCake(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, 255);
        // Flames fade out
        uint8_t flameAlpha = (uint8_t)((1.0f - ease::in(ph.p)) * 255);
        if (flameAlpha > 10) {
            drawFlames(gfx, cakeCx, cakeCy, ph.p, colors.eye, flameAlpha);
        }
        // Puff cloud expands
        if (ph.p > 0.15f) {
            float puffP = (ph.p - 0.15f) / 0.85f;
            drawPuffCloud(gfx, cakeCx, cakeCy, puffP, colors.eye);
        }
    } else if (phaseIs(ph.name, "confetti")) {
        drawCake(gfx, cakeCx, cakeCy, colors.eye, 255);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, 255);

        // Confetti burst: scattered rects and circles
        static const ToneId CONF_TONES[] = {
            TONE_WARM, TONE_ROSE, TONE_CYAN, TONE_GREEN, TONE_PURPLE, TONE_ORANGE
        };
        for (int i = 0; i < 16; i++) {
            float ip = fmodf(ph.p * 1.5f + (float)i * 0.062f, 1.0f);
            int16_t x = (int16_t)((i * 47 + (int)(sinf(ip * TAU + (float)i) * 6.0f)) % SCREEN_W);
            int16_t y = (int16_t)lerp((float)(STATUS_H + 10), (float)(SCREEN_H - 10), ip);
            uint8_t op = (uint8_t)((1.0f - ip) * 230);
            uint16_t pc = colors.tones[CONF_TONES[i % 6]];
            if (i % 2 == 0) {
                gfx.fillRect((int16_t)(x - 3), (int16_t)(y - 2), 6, 4, pc, op);
            } else {
                gfx.fillCircle(x, y, 3, pc, op);
            }
        }
    } else if (phaseIs(ph.name, "cakeOut")) {
        // Cake exits downward
        float ep = ease::in(ph.p);
        cakeCy = (int16_t)lerp((float)(CY + 20), (float)(SCREEN_H + 50), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawCake(gfx, cakeCx, cakeCy, colors.eye, alpha);
        drawCandles(gfx, cakeCx, cakeCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "candles", "blow", "confetti")) {
        drawActLabel(gfx, "BIRTHDAY", colors.eye);
    } else if (phaseIn(ph.name, "done", "wish")) {
        drawActLabel(gfx, "HAPPY BDAY", colors.eye);
    }
}

const VariantDef BIRTHDAY_VARIANTS[] = {
    {"birthday-cake", "Cake + blow", 14000, TONE_NONE, render_birthday},
};

extern const CategoryDef CAT_BIRTHDAY = {
    "birthday", "Birthday", ContentKind::SCENE, TONE_ROSE,
    BIRTHDAY_VARIANTS, sizeof(BIRTHDAY_VARIANTS) / sizeof(BIRTHDAY_VARIANTS[0]), false
};
