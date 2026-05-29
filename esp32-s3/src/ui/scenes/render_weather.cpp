#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Weather-window arc: curtains open → time-lapse sky → curtains close (12s)
static const PhaseEntry WEATHER_PHASES[] = {
    {"curious",      0.10f},
    {"curtainOpen",  0.10f},
    {"shrink",       0.06f},
    {"watch",        0.48f},
    {"curtainClose", 0.10f},
    {"grow",         0.06f},
    {"done",         0.10f},
};
static constexpr uint8_t WP_COUNT = sizeof(WEATHER_PHASES) / sizeof(WEATHER_PHASES[0]);

static void render_weather_window(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, WEATHER_PHASES, WP_COUNT);

    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "curious")) { eyeEmo = "curious"; }
    else if (phaseIs(ph.name, "curtainOpen")) { eyeEmo = "eager"; }
    else if (phaseIs(ph.name, "shrink")) {
        eyeCx = lerp((float)SCX, (float)(SCREEN_W - 42), ease::inOut(ph.p));
        eyeCy = lerp((float)CY, (float)(STATUS_H + 24), ease::inOut(ph.p));
        eyeScale = lerp(1.0f, 0.32f, ease::inOut(ph.p));
        eyeEmo = "happy";
    }
    else if (phaseIs(ph.name, "watch")) {
        eyeCx = (float)(SCREEN_W - 42); eyeCy = (float)(STATUS_H + 24);
        eyeScale = 0.32f; eyeEmo = "happy";
    }
    else if (phaseIs(ph.name, "curtainClose")) {
        eyeCx = (float)(SCREEN_W - 42); eyeCy = (float)(STATUS_H + 24);
        eyeScale = 0.32f; eyeEmo = "satisfied";
    }
    else if (phaseIs(ph.name, "grow")) {
        eyeCx = lerp((float)(SCREEN_W - 42), (float)SCX, ease::inOut(ph.p));
        eyeCy = lerp((float)(STATUS_H + 24), (float)CY, ease::inOut(ph.p));
        eyeScale = lerp(0.32f, 1.0f, ease::inOut(ph.p));
        eyeEmo = "satisfied";
    }
    else { eyeEmo = "satisfied"; }

    // Window frame
    int16_t winX = SCX - 90, winY = STATUS_H + 18;
    int16_t winW = 180, winH = SCREEN_H - STATUS_H - 50;

    // Curtain open factor
    float curt = 0.0f;
    if (phaseIs(ph.name, "curtainOpen")) curt = ease::out(ph.p);
    else if (phaseIs(ph.name, "curious")) curt = 0.0f;
    else if (phaseIs(ph.name, "curtainClose")) curt = 1.0f - ease::in(ph.p);
    else if (!phaseIs(ph.name, "done") && !phaseIs(ph.name, "grow")) curt = 1.0f;
    else curt = 0.0f;

    // Window frame lines
    gfx.strokeRect(winX, winY, winW, winH, colors.eye, 3);
    gfx.drawLine(winX, (int16_t)(winY + winH / 2),
                 (int16_t)(winX + winW), (int16_t)(winY + winH / 2),
                 colors.eye, 2, 178);
    gfx.drawLine((int16_t)(winX + winW / 2), winY,
                 (int16_t)(winX + winW / 2), (int16_t)(winY + winH),
                 colors.eye, 2, 178);

    // Sky inside window (when curtains open)
    if (curt > 0.05f) {
        float skyT = phaseIs(ph.name, "watch") ? ph.p : 0.0f;
        float sunX = (float)winX + 10.0f + skyT * (float)(winW - 20);
        float sunY = (float)winY + (float)winH * 0.6f
                     - sinf(skyT * PI) * (float)winH * 0.45f;

        // Sun
        if (phaseIs(ph.name, "watch") || phaseIs(ph.name, "curtainClose")) {
            gfx.fillCircle((int16_t)sunX, (int16_t)sunY, 14, colors.accent);
            for (int i = 0; i < 6; i++) {
                float a = (float)i / 6.0f * TAU + t * 0.5f;
                gfx.drawLine((int16_t)(sunX + cosf(a) * 16),
                             (int16_t)(sunY + sinf(a) * 16),
                             (int16_t)(sunX + cosf(a) * 22),
                             (int16_t)(sunY + sinf(a) * 22),
                             colors.accent, 2, 178);
            }
        }

        // Clouds drifting
        for (int i = 0; i < 3; i++) {
            float seed = i * 0.33f;
            float p = fmodf(skyT * 0.6f + seed, 1.0f);
            float cx = (float)winX - 20.0f + p * (float)(winW + 40);
            float cy = (float)winY + 30.0f + (i % 2) * 10.0f;
            gfx.fillEllipse((int16_t)cx, (int16_t)cy, 12, 6, colors.eye, 140);
            gfx.fillEllipse((int16_t)(cx + 8), (int16_t)(cy - 4), 9, 5, colors.eye, 140);
            gfx.fillEllipse((int16_t)(cx - 8), (int16_t)(cy - 3), 8, 4, colors.eye, 140);
        }
    }

    // Curtains (solid rects that slide apart)
    int16_t curtW = (int16_t)((float)(winW / 2 + 2) * (1.0f - curt));
    if (curtW > 0) {
        gfx.fillRect(winX - 2, winY - 4, curtW, winH + 8, colors.eye);
        gfx.fillRect((int16_t)(winX + winW / 2 + (float)(winW / 2) * curt),
                     winY - 4, curtW, winH + 8, colors.eye);
    }
    // Curtain rod
    gfx.drawLine((int16_t)(winX - 8), (int16_t)(winY - 6),
                 (int16_t)(winX + winW + 8), (int16_t)(winY - 6),
                 colors.accent, 3);

    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t, colors.eye, colors.bg);

    const char* label =
        phaseIs(ph.name, "curious")      ? "WHAT'S OUT THERE?" :
        phaseIs(ph.name, "curtainOpen")   ? "OPENING..." :
        phaseIs(ph.name, "shrink")        ? "LOOK OUTSIDE" :
        phaseIs(ph.name, "watch")         ? "SUNNY ALL DAY" :
        phaseIs(ph.name, "curtainClose")  ? "NIGHT FALLS" :
                                            "NICE DAY";
    drawActLabel(gfx, label, colors.eye);
}

const VariantDef WEATHER_VARIANTS[] = {
    {"weather-window", "Look outside", 12000, TONE_NONE, render_weather_window},
};

extern const CategoryDef CAT_WEATHER = {
    "weather", "Weather", ContentKind::SCENE, TONE_CYAN,
    WEATHER_VARIANTS, sizeof(WEATHER_VARIANTS) / sizeof(WEATHER_VARIANTS[0]), false
};
