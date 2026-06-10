#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// normal-steady: two eyes with periodic double-blink
static void render_normal_steady(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(blink(t, 0.32f, 0.07f), blink(t, 0.78f, 0.07f));
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-breathe: gentle scale + vertical bob
static void render_normal_breathe(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = 1.0f + sinf(t * TAU) * 0.05f;
    float dy = sinf(t * TAU) * 3.0f;
    float b = blink(t, 0.55f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H * s, 4.0f, b);
    int16_t w = (int16_t)(EYE_W * s);
    gfx.drawEye(LX, (int16_t)(CY + dy), w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), w, h, EYE_RX, 0, colors.eye);
}

// normal-look: eyes with bg-colored "pupil" circles that move in a sequence
static void render_normal_look(GfxEngine& gfx, float t, const ColorContext& colors) {
    struct Pos { float x, y; };
    static constexpr Pos seq[] = {{0,0},{-8,0},{0,0},{8,0},{0,-4},{0,0}};
    constexpr int n = 6;

    int idx = clamp((float)((int)(t * n)), 0.0f, (float)(n - 1));
    float sub = t * n - idx;
    int nxt = (idx + 1) % n;
    float e = ease::inOut(sub);
    float dx = lerp(seq[idx].x, seq[nxt].x, e);
    float dy = lerp(seq[idx].y, seq[nxt].y, e);

    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), (int16_t)(CY + dy), 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), (int16_t)(CY + dy), 11, colors.bg);
}

// normal-hum: horizontal sway + tilt + blink
static void render_normal_hum(GfxEngine& gfx, float t, const ColorContext& colors) {
    float sway = sinf(t * TAU) * 4.0f;
    float tilt = sinf(t * TAU) * 3.0f;
    float b = blink(t, 0.85f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye((int16_t)(LX + sway), CY, EYE_W, h, EYE_RX, tilt, colors.eye);
    gfx.drawEye((int16_t)(RX + sway), CY, EYE_W, h, EYE_RX, tilt, colors.eye);
}

// normal-twitch: micro-jump at t=0.62-0.66
static void render_normal_twitch(GfxEngine& gfx, float t, const ColorContext& colors) {
    int16_t tw = (t > 0.62f && t < 0.66f) ? 4 : 0;
    float b = blink(t, 0.4f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX + tw, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX + tw, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-tilt: whole face tilts side to side
static void render_normal_tilt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU) * 6.0f;
    float dy = cosf(t * TAU) * -2.0f;
    gfx.pushTransform();
    gfx.rotate(tilt, SCX, CY);
    gfx.drawEye(LX, (int16_t)(CY + dy), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.popTransform();
}

// normal-double-blink: three rapid blinks
static void render_normal_double_blink(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(fmaxf(blink(t, 0.42f, 0.05f), blink(t, 0.52f, 0.05f)),
                    blink(t, 0.88f, 0.06f));
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-drift: slow diagonal floating
static void render_normal_drift(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU) * 5.0f;
    float dy = cosf(t * TAU * 0.5f) * 3.0f;
    float b = blink(t, 0.3f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye((int16_t)(LX + dx), (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + dx), (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-search: pupils scan through a 7-point keyframe sequence (x+y)
static void render_normal_search(GfxEngine& gfx, float t, const ColorContext& colors) {
    static constexpr float sx[] = {0, -14, 14, 0, -8, 8, 0};
    static constexpr float sy[] = {0, -2, -2, 0, 6, 6, 0};
    constexpr int n = 7;
    int idx = (int)clamp((float)((int)(t * n)), 0.0f, (float)(n - 1));
    float e = ease::inOut(t * n - idx);
    int nxt = (idx + 1) % n;
    float dx = lerp(sx[idx], sx[nxt], e);
    float dy = lerp(sy[idx], sy[nxt], e);
    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), (int16_t)(CY + dy), 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), (int16_t)(CY + dy), 11, colors.bg);
}

// normal-look-up: pupils glance upward (eyes still) with a blink
static void render_normal_look_up(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = sinf(t * TAU - PI / 2.0f);
    float dy = phase < 0.0f ? phase * 22.0f : 0.0f;
    float b = blink(t, 0.9f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    if (h > 30) {
        gfx.fillCircle(LX, (int16_t)(CY + dy), 11, colors.bg);
        gfx.fillCircle(RX, (int16_t)(CY + dy), 11, colors.bg);
    }
}

// normal-tired-blink: frequent heavy blinks
static void render_normal_tired_blink(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(fmaxf(blink(t, 0.18f, 0.09f), blink(t, 0.5f, 0.09f)),
                    blink(t, 0.82f, 0.09f));
    int16_t h = (int16_t)lerp((float)EYE_H * 0.78f, 4.0f, b);
    gfx.drawEye(LX, CY + 8, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY + 8, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-stare: very still, late blink + asymmetric end twitch (right eye)
static void render_normal_stare(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tw = (t > 0.84f && t < 0.94f) ? sinf((t - 0.84f) / 0.1f * PI) * 6.0f : 0.0f;
    float b = blink(t, 0.96f, 0.04f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + tw), CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// normal-peek: pupils peek left/right through a 5-point sequence, with blink
static void render_normal_peek(GfxEngine& gfx, float t, const ColorContext& colors) {
    static constexpr float sx[] = {0, 14, 0, -14, 0};
    constexpr int n = 5;
    int idx = (int)clamp((float)((int)(t * n)), 0.0f, (float)(n - 1));
    float dx = lerp(sx[idx], sx[(idx + 1) % n], ease::inOut(t * n - idx));
    float b = blink(t, 0.55f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H * 0.9f, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    if (h > 30) {
        gfx.fillCircle((int16_t)(LX + dx), CY, 11, colors.bg);
        gfx.fillCircle((int16_t)(RX + dx), CY, 11, colors.bg);
    }
}

// normal-pendulum: pupils swing left-right (eyes always open)
static void render_normal_pendulum(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU) * 12.0f;
    gfx.drawEyes(colors.eye);
    gfx.fillCircle((int16_t)(LX + dx), CY, 11, colors.bg);
    gfx.fillCircle((int16_t)(RX + dx), CY, 11, colors.bg);
}

// normal-puff: single cheek-puff pulse — eyes squish, big cheek ellipses
static void render_normal_puff(GfxEngine& gfx, float t, const ColorContext& colors) {
    float puff = pulse(t, 0.5f, 0.18f);
    int16_t h = (int16_t)((float)EYE_H * (1.0f - puff * 0.3f));
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    if (puff > 0.05f) {
        uint8_t op = (uint8_t)(puff * 0.6f * 255.0f);
        gfx.fillEllipse(28, CY + 18, 20, 14, colors.accent, op);
        gfx.fillEllipse(SCREEN_W - 28, CY + 18, 20, 14, colors.accent, op);
    }
}

// normal-rem: open eyes with darting pupils (lissajous) — dreaming
static void render_normal_rem(GfxEngine& gfx, float t, const ColorContext& colors) {
    float a = sinf(t * TAU * 4.0f) * 10.0f;
    float bb = cosf(t * TAU * 5.0f) * 5.0f;
    int16_t h = (int16_t)((float)EYE_H * 0.85f);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.fillCircle((int16_t)(LX + a), (int16_t)(CY + bb), 10, colors.bg);
    gfx.fillCircle((int16_t)(RX + a), (int16_t)(CY + bb), 10, colors.bg);
}

// --- Category registration ---
const VariantDef NORMAL_VARIANTS[] = {
    {"normal-steady",       "Steady",       5200, TONE_NONE, render_normal_steady},
    {"normal-breathe",      "Breathe",      3800, TONE_NONE, render_normal_breathe},
    {"normal-look",         "Look around",  4400, TONE_NONE, render_normal_look},
    {"normal-hum",          "Hum sway",     3000, TONE_NONE, render_normal_hum},
    {"normal-twitch",       "Micro twitch", 4800, TONE_NONE, render_normal_twitch},
    {"normal-tilt",         "Curious tilt", 3600, TONE_NONE, render_normal_tilt},
    {"normal-double-blink", "Double blink", 4400, TONE_NONE, render_normal_double_blink},
    {"normal-drift",        "Slow drift",   6000, TONE_NONE, render_normal_drift},
    {"normal-search",       "Search",       4800, TONE_NONE, render_normal_search},
    {"normal-look-up",      "Look up",      4200, TONE_NONE, render_normal_look_up},
    {"normal-tired-blink",  "Tired blink",  3800, TONE_NONE, render_normal_tired_blink},
    {"normal-stare",        "Stare",        6800, TONE_NONE, render_normal_stare},
    {"normal-peek",         "Peek",         5200, TONE_NONE, render_normal_peek},
    {"normal-pendulum",     "Pendulum",     3400, TONE_NONE, render_normal_pendulum},
    {"normal-puff",         "Puff cheeks",  2800, TONE_NONE, render_normal_puff},
    {"normal-rem",          "REM sleep",    2400, TONE_NONE, render_normal_rem},
};

extern const CategoryDef CAT_NORMAL = {
    "normal", "Normal", ContentKind::EMOTION, TONE_CYAN,
    NORMAL_VARIANTS, sizeof(NORMAL_VARIANTS) / sizeof(NORMAL_VARIANTS[0])
};
