#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ── Phase table ─────────────────────────────────────────────────── */
static const PhaseEntry NOTIF_PHASES[] = {
    {"idle",    0.06f},   // Luni steady
    {"propIn",  0.08f},   // Badge slides in from top
    {"shrink",  0.06f},   // Eyes shrink to corner
    {"pulse1",  0.16f},   // Badge pulses with "1"
    {"pulse2",  0.14f},   // Badge bounces, glow intensifies
    {"check",   0.14f},   // Checkmark appears, badge turns "read"
    {"propOut", 0.08f},   // Badge exits upward
    {"grow",    0.06f},   // Eyes grow back
    {"done",    0.10f},   // Satisfied Luni
    {"pad",     0.12f},
};
static constexpr uint8_t PHASE_COUNT = sizeof(NOTIF_PHASES) / sizeof(NOTIF_PHASES[0]);

/* ── Helper: draw checkmark ──────────────────────────────────────── */
static void drawCheckmark(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float progress, uint16_t color, uint8_t alpha) {
    // Short leg then long leg of check
    int16_t x0 = (int16_t)(cx - 14);
    int16_t y0 = cy;
    int16_t x1 = (int16_t)(cx - 4);
    int16_t y1 = (int16_t)(cy + 10);
    int16_t x2 = (int16_t)(cx + 16);
    int16_t y2 = (int16_t)(cy - 10);

    if (progress < 0.5f) {
        float p = progress / 0.5f;
        int16_t ex = (int16_t)lerp((float)x0, (float)x1, p);
        int16_t ey = (int16_t)lerp((float)y0, (float)y1, p);
        gfx.drawLine(x0, y0, ex, ey, color, 4, alpha);
    } else {
        float p = (progress - 0.5f) / 0.5f;
        int16_t ex = (int16_t)lerp((float)x1, (float)x2, p);
        int16_t ey = (int16_t)lerp((float)y1, (float)y2, p);
        gfx.drawLine(x0, y0, x1, y1, color, 4, alpha);
        gfx.drawLine(x1, y1, ex, ey, color, 4, alpha);
    }
}

/* ── Main render ─────────────────────────────────────────────────── */
static void render_notification(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, NOTIF_PHASES, PHASE_COUNT);

    /* --- Eye state ------------------------------------------------ */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "propIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "pulse1", "pulse2")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "check")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "propOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIn(ph.name, "done", "pad")) {
        eyeEmo = "satisfied";
    }

    /* --- Badge prop ----------------------------------------------- */
    int16_t badgeCx = SCX, badgeCy = CY;

    if (phaseIs(ph.name, "propIn")) {
        // Slide in from top
        float ep = ease::out(ph.p);
        badgeCy = (int16_t)lerp((float)(STATUS_H - 50), (float)CY, ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        gfx.fillRoundedRect((int16_t)(badgeCx - 36), (int16_t)(badgeCy - 22),
                            72, 44, 14, colors.eye, alpha);
        gfx.drawText("1", badgeCx, (int16_t)(badgeCy + 6), colors.bg, 2,
                     GfxEngine::TextAlign::CENTER, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        gfx.fillRoundedRect((int16_t)(badgeCx - 36), (int16_t)(badgeCy - 22),
                            72, 44, 14, colors.eye);
        gfx.drawText("1", badgeCx, (int16_t)(badgeCy + 6), colors.bg, 2,
                     GfxEngine::TextAlign::CENTER);
    } else if (phaseIs(ph.name, "pulse1")) {
        // Badge pulses (scale oscillates)
        float scaleOsc = 1.0f + sinf(ph.p * TAU * 3.0f) * 0.08f;
        int16_t w = (int16_t)(72.0f * scaleOsc);
        int16_t h = (int16_t)(44.0f * scaleOsc);
        gfx.fillRoundedRect((int16_t)(badgeCx - w / 2), (int16_t)(badgeCy - h / 2),
                            w, h, 14, colors.eye);
        gfx.drawText("1", badgeCx, (int16_t)(badgeCy + 6), colors.bg, 2,
                     GfxEngine::TextAlign::CENTER);

        // Glow ring
        uint8_t ringAlpha = (uint8_t)(sinf(ph.p * TAU * 3.0f) * 60.0f + 60.0f);
        gfx.strokeRoundedRect((int16_t)(badgeCx - w / 2 - 4),
                              (int16_t)(badgeCy - h / 2 - 4),
                              w + 8, h + 8, 16, colors.eye, 2);
        (void)ringAlpha; // alpha applied via strokeRoundedRect limitations
    } else if (phaseIs(ph.name, "pulse2")) {
        // Badge bounces vertically
        float bounceY = sinf(ph.p * TAU * 2.5f) * 8.0f;
        int16_t by = (int16_t)((float)badgeCy + bounceY);
        gfx.fillRoundedRect((int16_t)(badgeCx - 36), (int16_t)(by - 22),
                            72, 44, 14, colors.eye);
        gfx.drawText("1", badgeCx, (int16_t)(by + 6), colors.bg, 2,
                     GfxEngine::TextAlign::CENTER);

        // Expanding glow rings
        for (int i = 0; i < 2; i++) {
            float rp = fmodf(ph.p * 2.0f + (float)i * 0.5f, 1.0f);
            uint8_t rAlpha = (uint8_t)((1.0f - rp) * 120);
            int16_t rr = (int16_t)(40.0f + rp * 20.0f);
            gfx.strokeCircle(badgeCx, by, rr, colors.eye, 2);
            (void)rAlpha;
        }
    } else if (phaseIs(ph.name, "check")) {
        // Badge stays, checkmark draws over it
        gfx.fillRoundedRect((int16_t)(badgeCx - 36), (int16_t)(badgeCy - 22),
                            72, 44, 14, colors.eye);

        // Fade number out, draw checkmark in
        if (ph.p < 0.3f) {
            uint8_t numAlpha = (uint8_t)((1.0f - ph.p / 0.3f) * 255);
            gfx.drawText("1", badgeCx, (int16_t)(badgeCy + 6), colors.bg, 2,
                         GfxEngine::TextAlign::CENTER, numAlpha);
        }
        if (ph.p > 0.2f) {
            float checkP = clamp((ph.p - 0.2f) / 0.6f, 0.0f, 1.0f);
            drawCheckmark(gfx, badgeCx, badgeCy, checkP, colors.bg, 255);
        }
    } else if (phaseIs(ph.name, "propOut")) {
        // Badge exits upward
        float ep = ease::in(ph.p);
        badgeCy = (int16_t)lerp((float)CY, (float)(STATUS_H - 60), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        gfx.fillRoundedRect((int16_t)(badgeCx - 36), (int16_t)(badgeCy - 22),
                            72, 44, 14, colors.eye, alpha);
        drawCheckmark(gfx, badgeCx, badgeCy, 1.0f, colors.bg, alpha);
    }

    /* --- Eyes ----------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label ---------------------------------------------------- */
    if (phaseIn(ph.name, "pulse1", "pulse2")) {
        drawActLabel(gfx, "NEW", colors.eye);
    } else if (phaseIs(ph.name, "check")) {
        drawActLabel(gfx, "READ", colors.eye);
    }
}

const VariantDef NOTIFICATION_VARIANTS[] = {
    {"notif-cards", "Card stack", 12000, TONE_NONE, render_notification},
};

extern const CategoryDef CAT_NOTIFICATION = {
    "notification", "Notification", ContentKind::SCENE, TONE_WARM,
    NOTIFICATION_VARIANTS, sizeof(NOTIFICATION_VARIANTS) / sizeof(NOTIFICATION_VARIANTS[0]),
    false
};
