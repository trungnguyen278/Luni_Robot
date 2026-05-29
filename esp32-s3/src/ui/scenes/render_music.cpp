#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Music-jam arc: speakers fade in → bars dance → fade out (12s)
static const PhaseEntry MUSIC_PHASES[] = {
    {"neutral", 0.08f},
    {"fadeIn",  0.10f},
    {"shrink",  0.05f},
    {"jam",     0.50f},
    {"fadeOut", 0.10f},
    {"grow",    0.05f},
    {"done",    0.12f},
};
static constexpr uint8_t MP_COUNT = sizeof(MUSIC_PHASES) / sizeof(MUSIC_PHASES[0]);

static void drawSpeaker(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint16_t bg, float bassPulse) {
    gfx.fillRoundedRect(cx - 22, cy - 40, 44, 88, 4, color);
    // Tweeter
    gfx.fillCircle(cx, cy - 18, 10, bg);
    gfx.fillCircle(cx, cy - 18, 5, color);
    // Woofer
    gfx.fillCircle(cx, cy + 18, 16, bg);
    int16_t wr = (int16_t)(9.0f + bassPulse * 2.0f);
    gfx.fillCircle(cx, cy + 18, wr, color);
}

static void render_music_jam(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, MUSIC_PHASES, MP_COUNT);

    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    float beat = phaseIs(ph.name, "jam") ? sinf(ph.p * TAU * 16.0f) : 0.0f;

    if (phaseIs(ph.name, "neutral")) { eyeEmo = "steady"; }
    else if (phaseIs(ph.name, "fadeIn")) { eyeEmo = "curious"; }
    else if (phaseIs(ph.name, "shrink")) {
        eyeCy = lerp((float)CY, (float)CY + 50.0f, ease::inOut(ph.p));
        eyeScale = lerp(1.0f, 0.42f, ease::inOut(ph.p));
        eyeEmo = "happy";
    }
    else if (phaseIs(ph.name, "jam")) {
        eyeCy = (float)CY + 50.0f + fabsf(beat) * 4.0f;
        eyeScale = 0.42f; eyeEmo = "happy";
    }
    else if (phaseIs(ph.name, "fadeOut")) {
        eyeCy = (float)CY + 50.0f; eyeScale = 0.42f; eyeEmo = "satisfied";
    }
    else if (phaseIs(ph.name, "grow")) {
        eyeCy = lerp((float)CY + 50.0f, (float)CY, ease::inOut(ph.p));
        eyeScale = lerp(0.42f, 1.0f, ease::inOut(ph.p));
        eyeEmo = "satisfied";
    }
    else { eyeEmo = "satisfied"; }

    // Speaker opacity
    float spkOp = 0.0f;
    if (phaseIs(ph.name, "fadeIn")) spkOp = ease::out(ph.p);
    else if (phaseIn(ph.name, "shrink", "jam")) spkOp = 1.0f;
    else if (phaseIs(ph.name, "fadeOut")) spkOp = 1.0f - ease::in(ph.p);

    float bassPulse = phaseIs(ph.name, "jam")
        ? fabsf(sinf(ph.p * TAU * 4.0f)) : 0.0f;

    // Left speaker
    if (spkOp > 0.01f) {
        gfx.pushAlpha((uint8_t)(spkOp * 255));
        drawSpeaker(gfx, 56, CY - 10, colors.eye, colors.bg, bassPulse);
        drawSpeaker(gfx, SCREEN_W - 56, CY - 10, colors.eye, colors.bg, bassPulse);
        gfx.popAlpha();
    }

    // EQ bars during jam
    if (phaseIs(ph.name, "jam")) {
        for (int i = 0; i < 8; i++) {
            float f1 = sinf(ph.p * TAU * 8.0f + i * 0.7f);
            float f2 = sinf(ph.p * TAU * 16.0f + i * 0.3f);
            int16_t h = (int16_t)(6.0f + fabsf(f1 * 14.0f + f2 * 8.0f));
            int16_t x = SCX - 56 + i * 16;
            gfx.fillRoundedRect(x - 5, CY - 30 - h / 2, 10, h, 3, colors.accent);
        }

        // Bass pulse ring
        int16_t ringR = (int16_t)(40.0f + bassPulse * 30.0f);
        float ringOp = 1.0f - bassPulse * 0.7f;
        gfx.pushAlpha((uint8_t)(ringOp * 255));
        gfx.strokeCircle(SCX, CY - 30, ringR, colors.accent, 2);
        gfx.popAlpha();

        // Floating notes
        for (int i = 0; i < 3; i++) {
            float p = fmodf(ph.p * 1.2f + i * 0.33f, 1.0f);
            float nx = 80.0f + i * 80.0f + sinf(p * TAU + i) * 8.0f;
            float ny = lerp((float)(SCREEN_H - 30), (float)(STATUS_H + 14), p);
            uint8_t nop = (uint8_t)((1.0f - p) * 255);
            int16_t nsz = (int16_t)lerp(2.0f, 1.0f, p);
            gfx.fillCircle((int16_t)nx, (int16_t)ny, 4, colors.accent, nop);
            gfx.drawLine((int16_t)(nx + 4), (int16_t)(ny - 10),
                         (int16_t)(nx + 4), (int16_t)ny,
                         colors.accent, 2, nop);
        }
    }

    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t, colors.eye, colors.bg);

    const char* label =
        phaseIs(ph.name, "neutral") ? "SILENCE..." :
        phaseIs(ph.name, "fadeIn")  ? "A SONG!" :
        phaseIs(ph.name, "shrink")  ? "TURN IT UP" :
        phaseIs(ph.name, "jam")     ? "JAMMING" :
        phaseIs(ph.name, "fadeOut") ? "ENCORE?" :
                                      "GREAT TUNE";
    drawActLabel(gfx, label, colors.eye);
}

const VariantDef MUSIC_VARIANTS[] = {
    {"music-jam", "Speaker jam", 12000, TONE_NONE, render_music_jam},
};

extern const CategoryDef CAT_MUSIC = {
    "music", "Music", ContentKind::SCENE, TONE_ROSE,
    MUSIC_VARIANTS, sizeof(MUSIC_VARIANTS) / sizeof(MUSIC_VARIANTS[0]), false
};
