#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Weather is a multi-case scene: one window-frame arc shared by every
// condition; the props drawn *inside* the window change per case. Eyes shrink
// to the corner and "watch" the sky through the window (cyan watcher, always).
// Cases + tones mirror ui_design/scenes-weather.jsx + scenes-arc-pack-1.jsx.
enum class WX : uint8_t { WINDOW, CLOUDY, RAINY, STORM, SNOW, FOG, WINDY };

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

// A 3-puff cloud cluster centred on (cx,cy).
static void drawCloud(GfxEngine& gfx, float cx, float cy, uint16_t c, uint8_t a) {
    gfx.fillEllipse((int16_t)cx, (int16_t)cy, 12, 6, c, a);
    gfx.fillEllipse((int16_t)(cx + 8), (int16_t)(cy - 4), 9, 5, c, a);
    gfx.fillEllipse((int16_t)(cx - 8), (int16_t)(cy - 3), 8, 4, c, a);
}

// Draw the condition-specific sky inside the window rect. `t` drives the
// continuous looping motion (rain/snow/wind); `skyT` (0..1 during watch) drives
// the slow sun arc for the WINDOW case.
static void drawWeather(GfxEngine& gfx, WX kind,
                        int16_t winX, int16_t winY, int16_t winW, int16_t winH,
                        float skyT, float t, const ColorContext& colors,
                        bool sunActive) {
    const int16_t ix = winX + 8, iw = winW - 16;
    const int16_t iy = winY + 6, ih = winH - 12;

    switch (kind) {
    case WX::WINDOW: {
        if (sunActive) {
            float sunX = (float)winX + 10.0f + skyT * (float)(winW - 20);
            float sunY = (float)winY + (float)winH * 0.6f
                         - sinf(skyT * PI) * (float)winH * 0.45f;
            gfx.fillCircle((int16_t)sunX, (int16_t)sunY, 14, colors.accent);
            for (int i = 0; i < 6; i++) {
                float a = (float)i / 6.0f * TAU + t * 0.5f;
                gfx.drawLine((int16_t)(sunX + cosf(a) * 16), (int16_t)(sunY + sinf(a) * 16),
                             (int16_t)(sunX + cosf(a) * 22), (int16_t)(sunY + sinf(a) * 22),
                             colors.accent, 2, 178);
            }
        }
        for (int i = 0; i < 3; i++) {
            float p = fmodf(skyT * 0.6f + i * 0.33f, 1.0f);
            float cx = (float)winX - 20.0f + p * (float)(winW + 40);
            float cy = (float)winY + 30.0f + (i % 2) * 10.0f;
            drawCloud(gfx, cx, cy, colors.eye, 140);
        }
        break;
    }
    case WX::CLOUDY: {
        // Overcast: 5 fuller clouds drifting slowly.
        for (int i = 0; i < 5; i++) {
            float p = fmodf(t * 0.05f + i * 0.21f, 1.0f);
            float cx = (float)winX - 20.0f + p * (float)(winW + 40);
            float cy = (float)iy + 16.0f + (i % 3) * 26.0f;
            drawCloud(gfx, cx, cy, colors.accent, 180);
        }
        break;
    }
    case WX::RAINY: {
        for (int i = 0; i < 3; i++)
            drawCloud(gfx, (float)ix + 24.0f + i * 56.0f, (float)iy + 16.0f, colors.accent, 200);
        for (int i = 0; i < 14; i++) {
            float p = fmodf(t * 1.6f + i * 0.137f, 1.0f);
            float x = (float)ix + 6.0f + fmodf(i * 21.0f, (float)(iw - 12));
            float y = (float)iy + 30.0f + p * (float)(ih - 36);
            gfx.drawLine((int16_t)x, (int16_t)y, (int16_t)(x - 2), (int16_t)(y + 9),
                         colors.accent, 2, 210);
        }
        break;
    }
    case WX::STORM: {
        for (int i = 0; i < 3; i++)
            drawCloud(gfx, (float)ix + 24.0f + i * 56.0f, (float)iy + 16.0f, colors.accent, 220);
        for (int i = 0; i < 9; i++) {
            float p = fmodf(t * 1.9f + i * 0.21f, 1.0f);
            float x = (float)ix + 8.0f + fmodf(i * 27.0f, (float)(iw - 16));
            float y = (float)iy + 30.0f + p * (float)(ih - 36);
            gfx.drawLine((int16_t)x, (int16_t)y, (int16_t)(x - 2), (int16_t)(y + 8),
                         colors.accent, 2, 190);
        }
        // Lightning bolt flashes briefly each cycle.
        if (fmodf(t * 2.6f, 1.0f) < 0.10f) {
            int16_t bx = (int16_t)(ix + iw * 0.55f), by = (int16_t)(iy + 30);
            gfx.drawLine(bx, by, (int16_t)(bx - 10), (int16_t)(by + 22), colors.accent, 3);
            gfx.drawLine((int16_t)(bx - 10), (int16_t)(by + 22), (int16_t)(bx + 4), (int16_t)(by + 28), colors.accent, 3);
            gfx.drawLine((int16_t)(bx + 4), (int16_t)(by + 28), (int16_t)(bx - 6), (int16_t)(by + 52), colors.accent, 3);
        }
        break;
    }
    case WX::SNOW: {
        for (int i = 0; i < 16; i++) {
            float p = fmodf(t * 0.7f + i * 0.097f, 1.0f);
            float baseX = (float)ix + 8.0f + fmodf(i * 19.0f, (float)(iw - 16));
            float x = baseX + sinf(p * TAU + (float)i) * 7.0f;
            float y = (float)iy + 6.0f + p * (float)(ih - 12);
            gfx.fillCircle((int16_t)x, (int16_t)y, 2, colors.accent, 230);
        }
        break;
    }
    case WX::FOG: {
        // Horizontal drifting fog bands (soft ellipses).
        for (int i = 0; i < 4; i++) {
            float p = fmodf(t * 0.18f + i * 0.25f, 1.0f);
            float xoff = sinf(p * TAU) * 12.0f;
            int16_t y = (int16_t)(iy + 24 + i * 34);
            gfx.fillEllipse((int16_t)(ix + iw / 2 + xoff), y, (int16_t)(iw / 2 - 6), 6, colors.accent, 90);
        }
        break;
    }
    case WX::WINDY: {
        for (int i = 0; i < 2; i++) {
            float p = fmodf(t * 0.22f + i * 0.5f, 1.0f);
            float cx = (float)winX - 20.0f + p * (float)(winW + 40);
            drawCloud(gfx, cx, (float)iy + 18.0f + i * 22.0f, colors.accent, 170);
        }
        for (int i = 0; i < 3; i++) {
            float p = fmodf(t * 1.1f + i * 0.33f, 1.0f);
            float x = (float)winX - 16.0f + p * (float)(winW + 32);
            int16_t y = (int16_t)(iy + 40 + i * 30);
            gfx.drawQuadBezier((int16_t)x, y, (int16_t)(x + 18), (int16_t)(y - 6),
                               (int16_t)(x + 36), y, colors.accent, 2);
        }
        break;
    }
    }
}

static void render_weather_core(GfxEngine& gfx, float t, const ColorContext& colors,
                                WX kind, const char* watchLabel) {
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

    int16_t winX = SCX - 90, winY = STATUS_H + 18;
    int16_t winW = 180, winH = SCREEN_H - STATUS_H - 50;

    float curt = 0.0f;
    if (phaseIs(ph.name, "curtainOpen")) curt = ease::out(ph.p);
    else if (phaseIs(ph.name, "curious")) curt = 0.0f;
    else if (phaseIs(ph.name, "curtainClose")) curt = 1.0f - ease::in(ph.p);
    else if (!phaseIs(ph.name, "done") && !phaseIs(ph.name, "grow")) curt = 1.0f;
    else curt = 0.0f;

    // Window frame
    gfx.strokeRect(winX, winY, winW, winH, colors.eye, 3);
    gfx.drawLine(winX, (int16_t)(winY + winH / 2),
                 (int16_t)(winX + winW), (int16_t)(winY + winH / 2), colors.eye, 2, 178);
    gfx.drawLine((int16_t)(winX + winW / 2), winY,
                 (int16_t)(winX + winW / 2), (int16_t)(winY + winH), colors.eye, 2, 178);

    // Sky inside the window once the curtains are open.
    if (curt > 0.05f) {
        float skyT = phaseIs(ph.name, "watch") ? ph.p : 0.0f;
        bool sunActive = phaseIs(ph.name, "watch") || phaseIs(ph.name, "curtainClose");
        drawWeather(gfx, kind, winX, winY, winW, winH, skyT, t, colors, sunActive);
    }

    // Curtains (solid rects that slide apart)
    int16_t curtW = (int16_t)((float)(winW / 2 + 2) * (1.0f - curt));
    if (curtW > 0) {
        gfx.fillRect(winX - 2, winY - 4, curtW, winH + 8, colors.eye);
        gfx.fillRect((int16_t)(winX + winW / 2 + (float)(winW / 2) * curt),
                     winY - 4, curtW, winH + 8, colors.eye);
    }
    gfx.drawLine((int16_t)(winX - 8), (int16_t)(winY - 6),
                 (int16_t)(winX + winW + 8), (int16_t)(winY - 6), colors.accent, 3);

    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t, TONE_LUT[TONE_CYAN], colors.bg);

    const char* label =
        phaseIs(ph.name, "curious")     ? "WHAT'S OUT THERE?" :
        phaseIs(ph.name, "curtainOpen") ? "OPENING..." :
        phaseIs(ph.name, "shrink")      ? "LOOK OUTSIDE" :
        phaseIs(ph.name, "watch")       ? watchLabel :
        phaseIs(ph.name, "curtainClose")? "CLOSING..." :
                                          "NICE DAY";
    drawActLabel(gfx, label, colors.eye);
}

static void render_weather_window(GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::WINDOW, "SUNNY ALL DAY"); }
static void render_weather_cloudy(GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::CLOUDY, "CLOUDY"); }
static void render_weather_rainy (GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::RAINY,  "RAINY"); }
static void render_weather_storm (GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::STORM,  "THUNDERSTORM"); }
static void render_weather_snow  (GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::SNOW,   "SNOW"); }
static void render_weather_fog   (GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::FOG,    "FOGGY"); }
static void render_weather_windy (GfxEngine& g, float t, const ColorContext& c) { render_weather_core(g, t, c, WX::WINDY,  "WINDY"); }

const VariantDef WEATHER_VARIANTS[] = {
    {"weather-window", "Look outside",  12000, TONE_NONE,   render_weather_window},
    {"weather-cloudy", "Cloudy",        12000, TONE_CYAN,   render_weather_cloudy},
    {"weather-rainy",  "Rainy",         12000, TONE_BLUE,   render_weather_rainy},
    {"weather-storm",  "Thunderstorm",  12000, TONE_PURPLE, render_weather_storm},
    {"weather-snow",   "Snow",          13000, TONE_WHITE,  render_weather_snow},
    {"weather-fog",    "Fog",           12000, TONE_BLUE,   render_weather_fog},
    {"weather-windy",  "Windy",         12000, TONE_CYAN,   render_weather_windy},
};

extern const CategoryDef CAT_WEATHER = {
    "weather", "Weather", ContentKind::SCENE, TONE_CYAN,
    WEATHER_VARIANTS, sizeof(WEATHER_VARIANTS) / sizeof(WEATHER_VARIANTS[0]), false
};
