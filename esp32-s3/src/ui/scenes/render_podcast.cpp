#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>
#include <cstdio>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry PODCAST_PHASES[] = {
    {"idle",     0.06f},   // Luni curious, centered
    {"micIn",    0.07f},   // Studio mic descends from top
    {"shrink",   0.05f},   // Eyes shrink to LEFT-BOTTOM corner
    {"onair",    0.07f},   // ON AIR sign lights up, blinking dot
    {"talk1",    0.14f},   // Waveform bars animate (talking)
    {"chapter",  0.04f},   // Chapter 1 marker appears
    {"talk2",    0.14f},   // More talking, waveform
    {"chapter2", 0.04f},   // Chapter 2 marker appears
    {"outro",    0.10f},   // Waveform fades, wrapping up
    {"offair",   0.07f},   // OFF AIR, sign dims
    {"micOut",   0.07f},   // Mic exits upward
    {"grow",     0.07f},   // Eyes grow back to center
    {"done",     0.08f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(PODCAST_PHASES) / sizeof(PODCAST_PHASES[0]);

/* ── Corner constants (LEFT-BOTTOM for this scene) ───────────────── */
static constexpr float CORNER_X = 36.0f;
static constexpr float CORNER_Y = (float)(SCREEN_H - 30);
static constexpr float CORNER_SCALE = 0.30f;

/* ── Waveform bar seed (deterministic pseudo-random heights) ─────── */
static float barSeed(int i, float t) {
    float base = sinf((float)i * 1.7f + t * TAU * 3.0f) * 0.5f + 0.5f;
    float mod  = sinf((float)i * 2.3f + t * TAU * 5.0f) * 0.3f;
    return clamp(base + mod, 0.08f, 1.0f);
}

/* ── Helper: draw studio microphone ──────────────────────────────── */
static void drawMicrophone(GfxEngine& gfx, int16_t cx, int16_t cy,
                           uint16_t color, uint8_t alpha) {
    // Stand (vertical line)
    gfx.drawLine(cx, (int16_t)(cy + 20), cx, (int16_t)(cy + 60), color, 3, alpha);
    // Stand base
    gfx.drawLine((int16_t)(cx - 18), (int16_t)(cy + 60),
                 (int16_t)(cx + 18), (int16_t)(cy + 60), color, 3, alpha);

    // Mount arm (horizontal)
    gfx.drawLine((int16_t)(cx - 6), (int16_t)(cy + 20),
                 (int16_t)(cx + 6), (int16_t)(cy + 20), color, 3, alpha);

    // Mic body (ellipse)
    gfx.fillEllipse(cx, cy, 12, 18, color, alpha);

    // Grille lines
    for (int i = -2; i <= 2; i++) {
        int16_t ly = (int16_t)(cy + i * 5);
        gfx.drawLine((int16_t)(cx - 8), ly, (int16_t)(cx + 8), ly,
                     COLOR_BG, 1, (uint8_t)(alpha / 2));
    }

    // Pop filter hint (arc)
    gfx.drawArc((int16_t)(cx + 22), cy, 14, PI * 0.6f, PI * 1.4f,
                color, 1, (uint8_t)(alpha / 3));
}

/* ── Helper: draw ON AIR badge ───────────────────────────────────── */
static void drawOnAirBadge(GfxEngine& gfx, int16_t cx, int16_t cy,
                           bool on, float blinkP,
                           uint16_t color, uint8_t alpha) {
    // Badge background
    gfx.fillRoundedRect((int16_t)(cx - 36), (int16_t)(cy - 10),
                        72, 20, 4, color, (uint8_t)(alpha / 4));
    gfx.strokeRoundedRect((int16_t)(cx - 36), (int16_t)(cy - 10),
                          72, 20, 4, color, 1);

    // Text
    const char* text = on ? "ON AIR" : "OFF AIR";
    uint8_t textOp = on ? alpha : (uint8_t)(alpha / 2);
    gfx.drawText(text, cx, (int16_t)(cy + 4), color, 1,
                 GfxEngine::TextAlign::CENTER, textOp);

    // Blinking dot (left of badge)
    if (on) {
        uint8_t dotOp = (uint8_t)(sinf(blinkP * TAU * 3.0f) * 100 + 155);
        gfx.fillCircle((int16_t)(cx - 28), cy, 3, TONE_LUT[TONE_RED],
                       (uint8_t)(dotOp * alpha / 255));
    }
}

/* ── Helper: draw waveform bars ──────────────────────────────────── */
static void drawWaveformBars(GfxEngine& gfx, int16_t left, int16_t right,
                             int16_t baseY, float t, float amplitude,
                             uint16_t color, uint8_t alpha) {
    int barCount = 18;
    int16_t totalW = right - left;
    int16_t gap = 2;
    int16_t barW = (totalW - gap * (barCount - 1)) / barCount;
    int16_t maxH = 50;

    for (int i = 0; i < barCount; i++) {
        float h = barSeed(i, t) * amplitude * (float)maxH;
        if (h < 2.0f) h = 2.0f;
        int16_t bh = (int16_t)h;
        int16_t bx = (int16_t)(left + i * (barW + gap));
        int16_t by = (int16_t)(baseY - bh / 2);
        gfx.fillRoundedRect(bx, by, barW, bh, 2, color, alpha);
    }
}

/* ── Helper: draw chapter badge ──────────────────────────────────── */
static void drawChapterBadge(GfxEngine& gfx, int16_t cx, int16_t cy,
                             int chapter, float fadeP,
                             uint16_t color) {
    uint8_t alpha = (uint8_t)(ease::out(fadeP) * 230);
    gfx.fillRoundedRect((int16_t)(cx - 32), (int16_t)(cy - 8),
                        64, 16, 3, color, (uint8_t)(alpha / 3));
    gfx.strokeRoundedRect((int16_t)(cx - 32), (int16_t)(cy - 8),
                          64, 16, 3, color, 1);

    char buf[12];
    snprintf(buf, sizeof(buf), "CH %d", chapter);
    gfx.drawText(buf, cx, (int16_t)(cy + 4), color, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_podcast(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, PODCAST_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "micIn")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, CORNER_X, ep);
        eyeCy    = lerp((float)CY,  CORNER_Y, ep);
        eyeScale = lerp(1.0f, CORNER_SCALE, ep);
        eyeEmo   = "eager";
    } else if (phaseIs(ph.name, "onair")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "excited";
    } else if (phaseIn(ph.name, "talk1", "talk2")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "happy";
    } else if (phaseIn(ph.name, "chapter", "chapter2")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "outro")) {
        eyeCx = CORNER_X; eyeCy = CORNER_Y; eyeScale = CORNER_SCALE;
        eyeEmo = "satisfied";
    } else if (phaseIn(ph.name, "offair", "micOut")) {
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

    /* --- Prop: Microphone ----------------------------------------- */
    int16_t micCx = SCX - 60, micCy = STATUS_H + 60;

    if (phaseIs(ph.name, "micIn")) {
        float ep = ease::out(ph.p);
        int16_t fromY = STATUS_H - 80;
        micCy = (int16_t)lerp((float)fromY, (float)(STATUS_H + 60), ep);
        drawMicrophone(gfx, micCx, micCy, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawMicrophone(gfx, micCx, micCy, colors.eye, 255);
    } else if (phaseIn(ph.name, "onair", "talk1", "chapter",
                       "talk2")) {
        drawMicrophone(gfx, micCx, micCy, colors.eye, 255);
    } else if (phaseIn(ph.name, "chapter2", "outro", "offair")) {
        drawMicrophone(gfx, micCx, micCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "micOut")) {
        float ep = ease::in(ph.p);
        micCy = (int16_t)lerp((float)(STATUS_H + 60), (float)(STATUS_H - 80), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawMicrophone(gfx, micCx, micCy, colors.eye, alpha);
    }

    /* --- ON AIR badge --------------------------------------------- */
    int16_t badgeX = SCX + 50, badgeY = STATUS_H + 32;

    if (phaseIs(ph.name, "onair")) {
        drawOnAirBadge(gfx, badgeX, badgeY, true, ph.p, colors.eye, 255);
    } else if (phaseIn(ph.name, "talk1", "chapter", "talk2",
                       "chapter2")) {
        drawOnAirBadge(gfx, badgeX, badgeY, true, t * 8.0f, colors.eye, 255);
    } else if (phaseIs(ph.name, "outro")) {
        float fadeOp = 1.0f - ease::in(ph.p) * 0.6f;
        drawOnAirBadge(gfx, badgeX, badgeY, true, t * 8.0f, colors.eye,
                       (uint8_t)(fadeOp * 255));
    } else if (phaseIs(ph.name, "offair")) {
        float fadeOp = 1.0f - ease::in(ph.p);
        drawOnAirBadge(gfx, badgeX, badgeY, false, 0.0f, colors.eye,
                       (uint8_t)(fadeOp * 255));
    }

    /* --- Waveform bars -------------------------------------------- */
    int16_t waveLeft = 80, waveRight = SCREEN_W - 20;
    int16_t waveBaseY = CY + 40;

    if (phaseIs(ph.name, "talk1")) {
        drawWaveformBars(gfx, waveLeft, waveRight, waveBaseY, t, 1.0f,
                         colors.eye, 200);
    } else if (phaseIs(ph.name, "chapter")) {
        drawWaveformBars(gfx, waveLeft, waveRight, waveBaseY, t, 0.4f,
                         colors.eye, 140);
        drawChapterBadge(gfx, SCX + 20, CY, 1, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "talk2")) {
        drawWaveformBars(gfx, waveLeft, waveRight, waveBaseY, t, 1.0f,
                         colors.eye, 200);
    } else if (phaseIs(ph.name, "chapter2")) {
        drawWaveformBars(gfx, waveLeft, waveRight, waveBaseY, t, 0.4f,
                         colors.eye, 140);
        drawChapterBadge(gfx, SCX + 20, CY, 2, ph.p, colors.eye);
    } else if (phaseIs(ph.name, "outro")) {
        float fade = 1.0f - ease::in(ph.p);
        drawWaveformBars(gfx, waveLeft, waveRight, waveBaseY, t,
                         fade * 0.6f, colors.eye, (uint8_t)(fade * 200));
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_PURPLE], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "onair", "talk1", "chapter", "talk2")) {
        drawActLabel(gfx, "PODCAST", colors.eye);
    } else if (phaseIn(ph.name, "chapter2", "outro")) {
        drawActLabel(gfx, "PODCAST", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "TUNED IN", colors.eye);
    }
}

const VariantDef PODCAST_VARIANTS[] = {
    {"podcast-main", "Main", 14000, TONE_NONE, render_podcast},
};

extern const CategoryDef CAT_PODCAST = {
    "podcast", "Podcast", ContentKind::SCENE, TONE_PURPLE,
    PODCAST_VARIANTS, sizeof(PODCAST_VARIANTS) / sizeof(PODCAST_VARIANTS[0]), false
};
