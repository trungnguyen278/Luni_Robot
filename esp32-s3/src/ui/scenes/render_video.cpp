#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry VIDEO_PHASES[] = {
    {"idle",      0.06f},   // Luni curious, centered
    {"tvIn",      0.07f},   // TV screen drops from top
    {"shrink",    0.05f},   // Eyes shrink to top-right corner
    {"play",      0.06f},   // Play button appears, pressed
    {"frame1",    0.14f},   // Scene 1 animated bars
    {"frame2",    0.14f},   // Scene 2 animated bars
    {"frame3",    0.14f},   // Scene 3 animated bars
    {"endcard",   0.08f},   // "THE END" text
    {"screenOut", 0.06f},   // TV screen exits down
    {"grow",      0.07f},   // Eyes grow back to center
    {"done",      0.13f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(VIDEO_PHASES) / sizeof(VIDEO_PHASES[0]);

/* ── Helper: draw TV frame ───────────────────────────────────────── */
static void drawTVFrame(GfxEngine& gfx, int16_t cx, int16_t cy,
                        uint16_t color, uint8_t alpha) {
    // Outer frame
    gfx.strokeRoundedRect((int16_t)(cx - 80), (int16_t)(cy - 50),
                          160, 100, 6, color, 3);
    // Screen bezel highlight
    gfx.strokeRoundedRect((int16_t)(cx - 76), (int16_t)(cy - 46),
                          152, 92, 4, color, 1);
    // Stand
    gfx.fillRect((int16_t)(cx - 20), (int16_t)(cy + 50), 40, 4, color, alpha);
    gfx.fillRect((int16_t)(cx - 2), (int16_t)(cy + 46), 4, 8, color, alpha);
}

/* ── Helper: draw play button triangle ───────────────────────────── */
static void drawPlayButton(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float scale, uint16_t color, uint8_t alpha) {
    int16_t sz = (int16_t)(18.0f * scale);
    // Circle background
    gfx.strokeCircle(cx, cy, (int16_t)(sz * 1.4f), color, 2);
    // Triangle
    gfx.fillTriangle((int16_t)(cx - sz / 2), (int16_t)(cy - sz),
                     (int16_t)(cx - sz / 2), (int16_t)(cy + sz),
                     (int16_t)(cx + sz), cy,
                     color, alpha);
}

/* ── Helper: draw animated frame bars (simulated video content) ── */
static void drawFrameBars(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float phase, int scene, uint16_t color) {
    int16_t sx = (int16_t)(cx - 68);
    int16_t sy = (int16_t)(cy - 38);
    int16_t sw = 136;
    int16_t sh = 76;

    // Dark screen background
    gfx.fillRect(sx, sy, sw, sh, COLOR_BG, 200);

    // Animated content bars (different patterns per scene)
    int barCount = 5 + scene * 2;
    for (int i = 0; i < barCount; i++) {
        float bPhase = fmodf(phase * 3.0f + (float)i * 0.15f, 1.0f);
        int16_t bw = (int16_t)(12 + sinf(bPhase * TAU + (float)i) * 8.0f);
        int16_t bh = (int16_t)(6 + fabsf(sinf(phase * TAU * 2.0f + (float)i * 0.8f)) * 10.0f);
        int16_t bx = (int16_t)(sx + 8 + (i * (sw - 16)) / barCount);
        int16_t by = (int16_t)(sy + sh / 2 - bh / 2 +
                     sinf(phase * TAU + (float)i * 1.2f) * 14.0f);
        by = (int16_t)clamp((float)by, (float)(sy + 4), (float)(sy + sh - bh - 4));
        bx = (int16_t)clamp((float)bx, (float)(sx + 4), (float)(sx + sw - bw - 4));
        uint8_t op = (uint8_t)(140 + sinf(phase * TAU * 1.5f + (float)i) * 80.0f);
        gfx.fillRoundedRect(bx, by, bw, bh, 2, color, op);
    }

    // Scene label
    const char* labels[] = {"SCENE 1", "SCENE 2", "SCENE 3"};
    int idx = clamp(scene - 1, 0, 2);
    gfx.drawText(labels[idx], cx, (int16_t)(sy + 12), color, 1,
                 GfxEngine::TextAlign::CENTER, 153);

    // Progress bar at bottom of screen area
    int16_t progY = (int16_t)(sy + sh - 6);
    gfx.fillRect(sx + 4, progY, (int16_t)((sw - 8) * phase), 3, color, 178);
    gfx.strokeRect(sx + 4, progY, sw - 8, 3, color, 1);
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_video(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, VIDEO_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "tvIn")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIn(ph.name, "play", "frame1")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIn(ph.name, "frame2", "frame3")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "endcard")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "screenOut")) {
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

    /* --- Prop: TV screen ------------------------------------------ */
    int16_t tvCx = SCX, tvCy = CY + 4;

    if (phaseIs(ph.name, "tvIn")) {
        float ep = ease::out(ph.p);
        int16_t fromY = STATUS_H - 80;
        tvCy = (int16_t)lerp((float)fromY, (float)(CY + 4), ep);
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, (uint8_t)(ep * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "play")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
        // Play button with pulse
        float pulse_s = 1.0f + sinf(ph.p * TAU) * 0.15f;
        uint8_t playAlpha = (uint8_t)(255 - ease::in(ph.p) * 200);
        drawPlayButton(gfx, tvCx, tvCy, pulse_s, colors.eye, playAlpha);
    } else if (phaseIs(ph.name, "frame1")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
        drawFrameBars(gfx, tvCx, tvCy, ph.p, 1, colors.eye);
    } else if (phaseIs(ph.name, "frame2")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
        drawFrameBars(gfx, tvCx, tvCy, ph.p, 2, colors.eye);
    } else if (phaseIs(ph.name, "frame3")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
        drawFrameBars(gfx, tvCx, tvCy, ph.p, 3, colors.eye);
    } else if (phaseIs(ph.name, "endcard")) {
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, 255);
        // Dark screen with "THE END"
        gfx.fillRect((int16_t)(tvCx - 68), (int16_t)(tvCy - 38),
                     136, 76, COLOR_BG, 200);
        float fadeIn = ease::out(clamp(ph.p * 2.0f, 0.0f, 1.0f));
        uint8_t textOp = (uint8_t)(fadeIn * 255);
        gfx.drawText("THE END", tvCx, tvCy + 4, colors.eye, 2,
                     GfxEngine::TextAlign::CENTER, textOp);
        // Star decoration
        for (int i = 0; i < 4; i++) {
            float angle = (float)i * (TAU / 4.0f) + ph.p * TAU * 0.3f;
            int16_t sx = (int16_t)(tvCx + cosf(angle) * 40.0f);
            int16_t sy = (int16_t)(tvCy + sinf(angle) * 22.0f);
            gfx.fillCircle(sx, sy, 2, colors.eye, textOp);
        }
    } else if (phaseIs(ph.name, "screenOut")) {
        float ep = ease::in(ph.p);
        tvCy = (int16_t)lerp((float)(CY + 4), (float)(SCREEN_H + 80), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawTVFrame(gfx, tvCx, tvCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_ROSE], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "play", "frame1", "frame2", "frame3")) {
        drawActLabel(gfx, "VIDEO", colors.eye);
    } else if (phaseIs(ph.name, "endcard")) {
        drawActLabel(gfx, "VIDEO", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ENJOYED", colors.eye);
    }
}

const VariantDef VIDEO_VARIANTS[] = {
    {"video-main", "Main", 14000, TONE_NONE, render_video},
};

extern const CategoryDef CAT_VIDEO = {
    "video", "Video", ContentKind::SCENE, TONE_ROSE,
    VIDEO_VARIANTS, sizeof(VIDEO_VARIANTS) / sizeof(VIDEO_VARIANTS[0]), false
};
