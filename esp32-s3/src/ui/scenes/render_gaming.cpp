#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry GAMING_PHASES[] = {
    {"idle",       0.06f},   // Bored Luni, centered
    {"lookAround", 0.07f},   // Eyes look around
    {"shrink",     0.05f},   // Eyes shrink to corner
    {"padIn",      0.07f},   // Gamepad rises from below
    {"press",      0.18f},   // Button presses, A/B flash
    {"score",      0.14f},   // "+1" popups float up
    {"boss",       0.14f},   // Boss fight, bigger flashes
    {"victory",    0.08f},   // "VICTORY!" text
    {"padOut",     0.06f},   // Gamepad exits downward
    {"grow",       0.07f},   // Eyes grow back
    {"done",       0.08f},   // Excited Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(GAMING_PHASES) / sizeof(GAMING_PHASES[0]);

/* -- Helper: draw gamepad ------------------------------------------------ */
static void drawGamepad(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint8_t alpha) {
    // Controller body
    gfx.fillRoundedRect((int16_t)(cx - 88), (int16_t)(cy - 22),
                        176, 48, 24, color, alpha);

    // D-pad cross (bg-colored)
    int16_t dx = (int16_t)(cx - 58);
    gfx.fillRoundedRect((int16_t)(dx - 18), (int16_t)(cy - 6), 36, 12, 2, COLOR_BG, alpha);
    gfx.fillRoundedRect((int16_t)(dx - 6), (int16_t)(cy - 18), 12, 36, 2, COLOR_BG, alpha);

    // A/B buttons (bg-colored circles)
    int16_t bx = (int16_t)(cx + 46);
    gfx.fillCircle((int16_t)(bx - 12), (int16_t)(cy + 6), 10, COLOR_BG, alpha);
    gfx.fillCircle((int16_t)(bx + 14), (int16_t)(cy - 6), 10, COLOR_BG, alpha);
    gfx.drawText("B", (int16_t)(bx - 12), (int16_t)(cy + 10), color, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
    gfx.drawText("A", (int16_t)(bx + 14), (int16_t)(cy - 2), color, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
}

/* -- Helper: draw button flash ------------------------------------------- */
static void drawButtonFlash(GfxEngine& gfx, int16_t cx, int16_t cy,
                            bool aBtn, float p, uint16_t color) {
    int16_t bx = (int16_t)(cx + 46);
    int16_t fx, fy;
    if (aBtn) { fx = (int16_t)(bx + 14); fy = (int16_t)(cy - 6); }
    else      { fx = (int16_t)(bx - 12); fy = (int16_t)(cy + 6); }

    uint8_t flash = (uint8_t)(sinf(p * TAU * 2.0f) * 100.0f + 155.0f);
    gfx.fillCircle(fx, fy, 10, color, flash);
}

/* -- Main render --------------------------------------------------------- */
static void render_gaming_play(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, GAMING_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "sleepy";
    } else if (phaseIs(ph.name, "lookAround")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "padIn", "press", "score")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "boss")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "victory")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "padOut")) {
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

    /* --- Prop: gamepad ------------------------------------------------- */
    int16_t padCx = SCX, padCy = CY + 10;

    if (phaseIs(ph.name, "padIn")) {
        // Rise from below
        float ep = ease::out(ph.p);
        padCy = (int16_t)lerp((float)(SCREEN_H + 40), (float)(CY + 10), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawGamepad(gfx, padCx, padCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawGamepad(gfx, padCx, padCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "press")) {
        // Button presses: alternate A/B with flashing
        drawGamepad(gfx, padCx, padCy, colors.eye, 255);
        bool aBtn = fmodf(ph.p * 6.0f, 2.0f) < 1.0f;
        drawButtonFlash(gfx, padCx, padCy, aBtn, ph.p, colors.eye);

        // D-pad direction highlight
        int dpadDir = ((int)(ph.p * 8.0f)) % 4;
        int16_t dx = (int16_t)(padCx - 58);
        if (dpadDir == 0) gfx.fillRect((int16_t)(dx - 6), (int16_t)(padCy - 18), 12, 12, colors.eye);
        if (dpadDir == 1) gfx.fillRect((int16_t)(dx + 6), (int16_t)(padCy - 6), 12, 12, colors.eye);
        if (dpadDir == 2) gfx.fillRect((int16_t)(dx - 6), (int16_t)(padCy + 6), 12, 12, colors.eye);
        if (dpadDir == 3) gfx.fillRect((int16_t)(dx - 18), (int16_t)(padCy - 6), 12, 12, colors.eye);
    } else if (phaseIs(ph.name, "score")) {
        // Gamepad stays, "+1" text floats upward
        drawGamepad(gfx, padCx, padCy, colors.eye, 255);

        for (int i = 0; i < 3; i++) {
            float sp = fmodf(ph.p * 2.0f + (float)i * 0.33f, 1.0f);
            int16_t tx = (int16_t)(padCx - 30 + i * 30);
            int16_t ty = (int16_t)(padCy - 40 - sp * 40.0f);
            uint8_t op = (uint8_t)((1.0f - sp) * 255);
            gfx.drawText("+1", tx, ty, colors.eye, 1,
                         GfxEngine::TextAlign::CENTER, op);
        }
    } else if (phaseIs(ph.name, "boss")) {
        // Bigger flashes, intense gameplay
        drawGamepad(gfx, padCx, padCy, colors.eye, 255);

        // Rapid button flashes on both A and B
        drawButtonFlash(gfx, padCx, padCy, true, ph.p * 2.0f, colors.eye);
        drawButtonFlash(gfx, padCx, padCy, false, ph.p * 2.0f + 0.5f, colors.eye);

        // Shake the whole pad
        float shake = sinf(ph.p * TAU * 8.0f) * 4.0f;
        (void)shake; // pad already drawn at rest; shake is visual via button flashes

        // Exclamation marks
        uint8_t exOp = (uint8_t)(sinf(ph.p * TAU * 4.0f) * 80.0f + 175.0f);
        gfx.drawText("!!", (int16_t)(padCx - 60), (int16_t)(padCy - 46), colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, exOp);
        gfx.drawText("!!", (int16_t)(padCx + 60), (int16_t)(padCy - 46), colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, exOp);
    } else if (phaseIs(ph.name, "victory")) {
        drawGamepad(gfx, padCx, padCy, colors.eye, 255);

        // "VICTORY!" text
        float scaleV = 1.0f + sinf(ph.p * TAU * 1.5f) * 0.15f;
        (void)scaleV;
        uint8_t vOp = (uint8_t)(ease::out(ph.p) * 255);
        gfx.drawText("VICTORY!", SCX, STATUS_H + 28, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, vOp);

        // Sparkle particles
        for (int i = 0; i < 6; i++) {
            float a = (float)i / 6.0f * TAU + ph.p * TAU;
            float r = 50.0f + ph.p * 20.0f;
            int16_t sx = (int16_t)(SCX + cosf(a) * r);
            int16_t sy = (int16_t)((float)(CY - 10) + sinf(a) * r * 0.5f);
            uint8_t sp = (uint8_t)((1.0f - fmodf(ph.p * 2.0f + (float)i * 0.16f, 1.0f)) * 200);
            gfx.fillCircle(sx, sy, 3, colors.eye, sp);
        }
    } else if (phaseIs(ph.name, "padOut")) {
        // Gamepad exits downward
        float ep = ease::in(ph.p);
        padCy = (int16_t)lerp((float)(CY + 10), (float)(SCREEN_H + 50), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawGamepad(gfx, padCx, padCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "press", "score")) {
        drawActLabel(gfx, "GAMING", colors.eye);
    } else if (phaseIs(ph.name, "boss")) {
        drawActLabel(gfx, "BOSS FIGHT", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "GG!", colors.eye);
    }
}

const VariantDef GAMING_VARIANTS[] = {
    {"gaming-play", "Play", 14000, TONE_NONE, render_gaming_play},
};

extern const CategoryDef CAT_GAMING = {
    "gaming", "Gaming", ContentKind::SCENE, TONE_PURPLE,
    GAMING_VARIANTS, sizeof(GAMING_VARIANTS) / sizeof(GAMING_VARIANTS[0]), false
};
