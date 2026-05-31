/* Luni — MOON scene (port of ui_design/scenes-moon.jsx).

   Luni *IS* the moon. Each lunar phase is its own long vignette that OPENS
   and CLOSES on Luni's signature cyan eyes: the centered pair shows first,
   the moon disc materializes around those same eyes (they shrink onto the
   disc), the story plays, the disc dissolves, and the eyes return.

       signature eyes → moon forms → STORY → moon dissolves → signature eyes

   The 8 variants (one per canonical phase, in MOON_SCENE_IDS order):
     moon-new      Sóc        ngủ say — sao băng vụt qua
     moon-wax-cre  lưỡi liềm  bừng tỉnh
     moon-first-q  thượng h.  leo dốc trời
     moon-wax-gib  khuyết đầu háo hức, đèn lồng thắp dần
     moon-full     RẰM        tròn đầy, an nhiên — đèn lồng bay
     moon-wan-gib  khuyết c.  mãn nguyện, đèn trôi đi
     moon-last-q   hạ huyền   ngáp dài, buồn ngủ
     moon-wan-cre  liềm tàn   về nghỉ, bình minh ló

   The host picks WHICH variant by tonight's lunar day — see
   SceneManager::pickMoonVariant() (mirrors LuniMoon.sceneForLunarDay).
*/
#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include "display/ColorSystem.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Luni's eyes are ALWAYS cyan — the robot's identity, present at the start
// and end of every story regardless of the scene tone.
static const uint16_t LUNI_CYAN = TONE_LUT[TONE_CYAN];

/* ----- progress helpers (mirror jsx seg/pulse) ------------------------- */
static inline float seg(float sp, float a, float b) {
    return clamp((sp - a) / (b - a), 0.0f, 1.0f);
}
static inline float pulseAB(float sp, float a, float b) {  // jsx pulse(sp,a,b)
    return sinf(seg(sp, a, b) * PI);
}
static inline uint8_t a8(float f) {  // 0..1 → 0..255, clamped
    return (uint8_t)(clamp(f, 0.0f, 1.0f) * 255.0f);
}

/* ----- small props ----------------------------------------------------- */
static void mTwinkle(GfxEngine& gfx, int16_t x, int16_t y, float r,
                     float t, float off, uint16_t col) {
    float tw = 0.55f + 0.45f * sinf(t * TAU * 0.9f + off);
    uint8_t a = a8(0.4f + tw * 0.5f);
    int16_t rr = (int16_t)fmaxf(1.0f, r);
    gfx.fillCircle(x, y, (int16_t)fmaxf(1.0f, r * 0.4f), col, a);
    gfx.drawLine((int16_t)(x - rr), y, (int16_t)(x + rr), y, col, 1, a);
    gfx.drawLine(x, (int16_t)(y - rr), x, (int16_t)(y + rr), col, 1, a);
}

static void mZzz(GfxEngine& gfx, float x, float y, float t,
                 float s, float op, uint16_t col) {
    for (int i = 0; i < 3; i++) {
        float p = fmodf(t * 0.5f + i * 0.34f, 1.0f);
        uint8_t a = a8((1.0f - p) * 0.85f * op);
        if (a < 8) continue;
        int16_t zx = (int16_t)(x + i * 7.0f * s + p * 16.0f * s);
        int16_t zy = (int16_t)(y - p * 22.0f * s);
        gfx.drawText("z", zx, zy, col, (i >= 1) ? 2 : 1,
                     GfxEngine::TextAlign::LEFT, a);
    }
}

static void mLantern(GfxEngine& gfx, float x, float y, float s, float lit,
                     const ColorContext& colors) {
    int16_t bx = (int16_t)x, by = (int16_t)y;
    uint16_t acc = colors.accent, eye = colors.eye;
    uint8_t bodyA = a8(0.3f + lit * 0.5f);
    uint8_t litA  = a8(lit);
    gfx.fillRoundedRect((int16_t)(bx - 7 * s), (int16_t)(by - 12 * s),
                        (int16_t)(14 * s), (int16_t)fmaxf(2.0f, 3 * s), 1, eye);
    gfx.fillEllipse(bx, by, (int16_t)(9 * s), (int16_t)(12 * s), acc, bodyA);
    gfx.fillCircle(bx, (int16_t)(by + 1 * s), (int16_t)fmaxf(1.0f, 3.2f * s), eye, litA);
    gfx.fillRoundedRect((int16_t)(bx - 5 * s), (int16_t)(by + 11 * s),
                        (int16_t)(10 * s), (int16_t)fmaxf(2.0f, 3 * s), 1, eye);
    gfx.drawLine(bx, (int16_t)(by + 14 * s), bx, (int16_t)(by + 19 * s), acc, 1, litA);
}

/* ----- Luni-as-moon disc: phase-correct (mirrors jsx moonDisc math).
        Lit side = scene tone; dark sphere + rim so Luni is always present
        even at Sóc. Drawn by scanline (no path-fill primitive). ----- */
static void drawMoonDisc(GfxEngine& gfx, int16_t cx, int16_t cy, int16_t R,
                         float p, uint8_t alpha, const ColorContext& colors) {
    if (R < 2) return;
    float cosv   = cosf(TAU * p);
    float illum  = (1.0f - cosv) * 0.5f;
    bool  waxing = p < 0.5f;
    bool  gibbous = illum > 0.5f;
    float rxFull = R * fabsf(cosv);
    uint16_t dark   = rgb565(14, 20, 34);   // #0e1422
    uint16_t accent = colors.accent;

    // soft halo / earthshine behind the disc
    gfx.fillCircle(cx, cy, (int16_t)(R * 1.28f), accent, (uint8_t)(alpha * 0.06f));

    for (int16_t dy = -R; dy <= R; dy++) {
        float yy = (float)dy / (float)R;
        float k  = 1.0f - yy * yy;
        if (k <= 0.0f) continue;
        float chord = sqrtf(k);
        float hw    = R * chord;
        int16_t y   = (int16_t)(cy + dy);
        int16_t left = (int16_t)(cx - hw);
        int16_t w    = (int16_t)(2.0f * hw) + 1;

        gfx.fillRect(left, y, w, 1, dark, alpha);                 // dark sphere
        if (illum < 0.05f)                                        // faint presence at Sóc
            gfx.fillRect(left, y, w, 1, accent, (uint8_t)(alpha * 0.10f));

        if (illum > 0.012f) {                                     // base lit half
            if (waxing) gfx.fillRect(cx, y, (int16_t)hw + 1, 1, accent, alpha);
            else        gfx.fillRect(left, y, (int16_t)hw + 1, 1, accent, alpha);
        }
        if (rxFull > 0.4f) {                                      // terminator band
            float ex = rxFull * chord;
            gfx.fillRect((int16_t)(cx - ex), y, (int16_t)(2.0f * ex) + 1, 1,
                         gibbous ? accent : dark, alpha);
        }
    }

    if (illum > 0.42f) {                                          // craters when lit
        gfx.fillCircle((int16_t)(cx - R * 0.30f), (int16_t)(cy - R * 0.35f),
                       (int16_t)(R * 0.12f), colors.bg, (uint8_t)(alpha * 0.5f));
        gfx.fillCircle((int16_t)(cx + R * 0.32f), (int16_t)(cy + R * 0.28f),
                       (int16_t)(R * 0.15f), colors.bg, (uint8_t)(alpha * 0.5f));
    }
    gfx.strokeCircle(cx, cy, R, colors.eye, 2);                   // rim
}

/* ============================================================
   Per-phase eye emotion (jsx moon emotions → firmware vocabulary)
   sleep/doze/yawn→sleepy, beam→happy, content/serene→satisfied,
   eager→eager, wide→surprised, climb/wake→steady.
============================================================ */
static const char* eye_new   (float, float)        { return "sleepy"; }
static const char* eye_waxcre(float, float sp)      { return sp < 0.16f ? "sleepy"
                                                           : sp < 0.30f ? "surprised"
                                                           : sp < 0.62f ? "steady" : "happy"; }
static const char* eye_firstq(float, float)         { return "steady"; }
static const char* eye_waxgib(float, float)         { return "eager"; }
static const char* eye_full  (float, float)         { return "satisfied"; }
static const char* eye_wangib(float, float)         { return "satisfied"; }
static const char* eye_lastq (float, float)         { return "sleepy"; }
static const char* eye_wancre(float, float)         { return "sleepy"; }

/* ----- per-phase disc path (dx,dy,scale) ------------------------------- */
static void path_new   (float sp, float, float& dx, float& dy, float& s) { (void)dx;(void)dy; s = 1.0f + pulseAB(sp, 0.55f, 0.72f) * 0.03f; }
static void path_waxcre(float sp, float, float& dx, float& dy, float& s) { (void)dx;(void)dy; s = 1.0f + pulseAB(sp, 0.50f, 0.66f) * 0.05f; }
static void path_firstq(float sp, float, float& dx, float& dy, float& s) { float e = ease::inOut(sp); dx = lerp(-44.0f, 44.0f, e); dy = lerp(30.0f, -26.0f, e); s = 1.0f; }
static void path_waxgib(float,    float t, float& dx, float& dy, float& s) { dx = 0; dy = fabsf(sinf(t * TAU * 0.9f)) * -6.0f; s = 1.0f; }
static void path_wangib(float,    float t, float& dx, float& dy, float& s) { dx = sinf(t * TAU * 0.25f) * 6.0f; dy = 0; s = 1.0f; }
static void path_wancre(float sp, float, float& dx, float& dy, float& s) { dx = 0; dy = lerp(-4.0f, 26.0f, ease::in(sp)); s = lerp(1.0f, 0.92f, sp); }

/* ----- per-phase ambient props ----------------------------------------- */
static const int16_t MSTARS[8][2] = {
    {44, 64}, {104, 44}, {212, 50}, {276, 92}, {60, 150}, {292, 150}, {150, 38}, {188, 150}
};

static void extras_new(GfxEngine& gfx, float t, float sp, float cx, float cy, float R, const ColorContext& c) {
    uint16_t eye = c.eye;
    for (int i = 0; i < 8; i++) mTwinkle(gfx, MSTARS[i][0], MSTARS[i][1], 3.2f, t, i * 1.3f, eye);
    float sh = seg(sp, 0.18f, 0.42f);                              // shooting star
    if (sh > 0.02f && sh < 0.98f) {
        uint8_t a = a8(sinf(sh * PI));
        float shx = lerp(40.0f, 280.0f, sh), shy = lerp(46.0f, 150.0f, sh);
        gfx.drawLine((int16_t)(shx - 30), (int16_t)(shy - 15), (int16_t)shx, (int16_t)shy, eye, 2, a);
        gfx.fillCircle((int16_t)shx, (int16_t)shy, 3, eye, a);
    }
    mZzz(gfx, cx + R * 0.3f, cy - R * 1.0f, t * 0.7f, 1.1f, 1.0f - pulseAB(sp, 0.55f, 0.74f) * 0.8f, eye);
}

static void extras_waxcre(GfxEngine& gfx, float t, float sp, float, float, float, const ColorContext& c) {
    mTwinkle(gfx, 70, 60, 4.0f + pulseAB(sp, 0.6f, 0.85f) * 3.0f, t, 0.0f, c.eye);
    mTwinkle(gfx, 258, 80, 3.0f, t, 2.2f, c.eye);
}

static void extras_firstq(GfxEngine& gfx, float t, float, float, float, float, const ColorContext& c) {
    uint16_t acc = c.accent;
    for (int i = 0; i < 3; i++) {                                  // chevrons climbing
        float pp = fmodf(t * 0.6f + i * 0.33f, 1.0f);
        float y  = lerp((float)(SCREEN_H - 10), (float)(STATUS_H + 30), pp);
        uint8_t o = a8(sinf(pp * PI) * 0.45f);
        float lx = SCREEN_W * 0.15f, rx = SCREEN_W * 0.79f;
        gfx.drawLine((int16_t)lx, (int16_t)y, (int16_t)(lx + 7), (int16_t)(y - 7), acc, 2, o);
        gfx.drawLine((int16_t)(lx + 7), (int16_t)(y - 7), (int16_t)(lx + 14), (int16_t)y, acc, 2, o);
        gfx.drawLine((int16_t)rx, (int16_t)y, (int16_t)(rx + 7), (int16_t)(y - 7), acc, 2, o);
        gfx.drawLine((int16_t)(rx + 7), (int16_t)(y - 7), (int16_t)(rx + 14), (int16_t)y, acc, 2, o);
    }
    mTwinkle(gfx, 60, 60, 2.8f, t, 1.0f, c.eye);
    mTwinkle(gfx, 290, 150, 2.6f, t, 3.0f, c.eye);
}

static void extras_waxgib(GfxEngine& gfx, float t, float sp, float cx, float cy, float R, const ColorContext& c) {
    uint16_t acc = c.accent;
    for (int i = 0; i < 9; i++) {                                  // sparkles orbiting
        float on = clamp(sp * 1.4f - i / 12.0f, 0.0f, 1.0f);
        if (on <= 0.05f) continue;
        float a  = (float)i / 9.0f * TAU + t * 0.4f;
        float rr = R + 16.0f + sinf(t * TAU * 0.8f + i) * 5.0f;
        gfx.fillCircle((int16_t)(cx + cosf(a) * rr), (int16_t)(cy + sinf(a) * rr),
                       (int16_t)fmaxf(1.0f, 2.6f * on), acc, a8(on * 0.85f));
    }
    const float LX4[4] = {70, 128, 196, 254};                      // lanterns light up
    for (int i = 0; i < 4; i++) {
        float lit = clamp((sp - (i * 0.2f + 0.05f)) * 6.0f, 0.0f, 1.0f);
        if (lit <= 0.02f) continue;
        mLantern(gfx, LX4[i], SCREEN_H - 26 + sinf(t * TAU * 0.6f + i) * 2.0f, 0.78f, lit, c);
    }
}

static void extras_full(GfxEngine& gfx, float t, float, float cx, float cy, float R, const ColorContext& c) {
    uint16_t acc = c.accent;
    gfx.fillCircle((int16_t)cx, (int16_t)cy, (int16_t)(R * (1.32f + 0.06f * sinf(t * TAU * 0.3f))), acc, a8(0.07f));
    gfx.fillCircle((int16_t)cx, (int16_t)cy, (int16_t)(R * (1.14f + 0.04f * sinf(t * TAU * 0.3f))), acc, a8(0.06f));
    for (int i = 0; i < 6; i++) {                                  // fireflies
        float a  = (float)i / 6.0f * TAU + t * 0.18f;
        float rr = R * 1.45f + sinf(t * TAU * 0.4f + i) * 10.0f;
        float tw = 0.4f + 0.6f * fabsf(sinf(t * TAU * 0.5f + i));
        gfx.fillCircle((int16_t)(cx + cosf(a) * rr), (int16_t)(cy + sinf(a) * rr * 0.7f), 2, acc, a8(tw * 0.7f));
    }
    for (int i = 0; i < 2; i++) {                                  // two lanterns rising
        int sgn = (i == 0) ? -1 : 1;
        float ph = fmodf(t * 0.1f + i * 0.5f, 1.0f);
        mLantern(gfx, cx + sgn * (R + 34), lerp((float)(SCREEN_H + 16), (float)(STATUS_H + 26), ph), 0.82f, 1.0f, c);
    }
}

static void extras_wangib(GfxEngine& gfx, float t, float sp, float cx, float, float R, const ColorContext& c) {
    mTwinkle(gfx, 64, 60, 2.6f, t, 1.5f, c.eye);
    mTwinkle(gfx, 286, 150, 2.4f, t, 3.4f, c.eye);
    for (int i = 0; i < 2; i++) {                                  // lanterns drift away
        if (seg(sp, 0.1f + i * 0.25f, 1.0f) >= 1.0f) continue;
        float dx = lerp(cx - R - 10.0f, -40.0f, seg(sp, 0.1f + i * 0.25f, 0.85f));
        float yy = SCREEN_H - 40 + sinf(t * TAU * 0.5f + i) * 6.0f;
        mLantern(gfx, dx, yy, 0.74f, clamp(0.7f - sp * 0.4f, 0.0f, 1.0f), c);
    }
}

static void extras_lastq(GfxEngine& gfx, float t, float sp, float cx, float cy, float R, const ColorContext& c) {
    mTwinkle(gfx, 58, 58, 2.6f, t, 0.0f, c.eye);
    mTwinkle(gfx, 150, 44, 2.2f, t, 1.7f, c.eye);
    mTwinkle(gfx, 286, 74, 2.8f, t, 3.1f, c.eye);
    if (sp > 0.55f) mZzz(gfx, cx + R * 0.4f, cy - R * 1.0f, t, 0.9f, seg(sp, 0.55f, 0.75f), c.eye);
}

static void extras_wancre(GfxEngine& gfx, float t, float sp, float cx, float cy, float R, const ColorContext& c) {
    gfx.fillRect(0, (int16_t)(SCREEN_H - 30), SCREEN_W, 30, c.accent,        // dawn band
                 a8(0.06f + seg(sp, 0.3f, 1.0f) * 0.12f));
    mTwinkle(gfx, 96, 64, 2.4f * (1.0f - sp * 0.7f), t, 1.2f, c.eye);
    mTwinkle(gfx, 232, 56, 2.2f * (1.0f - sp * 0.7f), t, 2.6f, c.eye);
    if (sp < 0.5f) mZzz(gfx, cx + R * 0.4f, cy - R * 0.95f, t, 0.8f, 1.0f - seg(sp, 0.2f, 0.5f), c.eye);
}

/* ============================================================
   Shared skeleton: bookend on signature eyes; moon forms around them;
   the long `act` carries the story (sp = 0..1); then it dissolves.
============================================================ */
typedef const char* (*EyeForFn)(float t, float sp);
typedef void (*PathFn)(float sp, float t, float& dx, float& dy, float& s);
typedef void (*ExtrasFn)(GfxEngine&, float t, float sp, float cx, float cy, float R, const ColorContext&);

struct MoonPhaseCfg {
    float    p;            // phase position 0..1 (drives disc illumination)
    float    baseR;        // disc radius
    float    breathe;      // breathe amplitude
    uint16_t durationMs;   // must match the VariantDef duration (for bookend timing)
    EyeForFn eyeFor;
    PathFn   pathFn;       // may be nullptr
    ExtrasFn extrasFn;     // may be nullptr
};

static void renderMoonStory(GfxEngine& gfx, float t, const ColorContext& colors,
                            const MoonPhaseCfg& cfg) {
    float durSec  = cfg.durationMs / 1000.0f;
    float fIn = 1.7f / durSec, fForm = 1.4f / durSec;
    float fOut = 1.7f / durSec, fUnform = 1.4f / durSec;
    float t1 = fIn, t2 = t1 + fForm, t4 = 1.0f - fOut, t3 = t4 - fUnform;

    float m, discOp, sp;
    const char* emo = "steady";
    float dcx = (float)SCX, dcy = (float)CY, R0 = cfg.baseR;
    bool inStory = false;

    if (t < t1) {                                  // signature eyes (open)
        m = 0; discOp = 0; sp = 0;
    } else if (t < t2) {                           // moon forms around eyes
        float pp = ease::inOut((t - t1) / (t2 - t1));
        m = pp; discOp = ease::out(pp); sp = 0;
        R0 = cfg.baseR * lerp(0.84f, 1.0f, pp);
    } else if (t < t3) {                           // the STORY
        inStory = true;
        m = 1; discOp = 1; sp = (t - t2) / (t3 - t2);
        emo = cfg.eyeFor ? cfg.eyeFor(t, sp) : "steady";
        float dx = 0, dy = 0, s = 1.0f;
        if (cfg.pathFn) cfg.pathFn(sp, t, dx, dy, s);
        dcx += dx; dcy += dy; R0 = cfg.baseR * s;
        R0 *= 1.0f + sinf(t * TAU * 0.4f) * cfg.breathe;
        dcy += sinf(t * TAU * 0.3f) * 3.0f;
    } else if (t < t4) {                           // moon dissolves
        float pp = ease::inOut((t - t3) / (t4 - t3));
        m = 1.0f - pp; discOp = 1.0f - ease::in(pp); sp = 1;
        R0 = cfg.baseR * lerp(1.0f, 0.84f, pp);
    } else {                                       // signature eyes (close)
        m = 0; discOp = 0; sp = 1;
    }

    if (cfg.extrasFn && inStory)
        cfg.extrasFn(gfx, t, sp, dcx, dcy, R0, colors);

    if (discOp > 0.01f)
        drawMoonDisc(gfx, (int16_t)dcx, (int16_t)dcy, (int16_t)R0, cfg.p, a8(discOp), colors);

    // eyes — always cyan — morph from centered signature pose onto the disc
    float eyeScale = lerp(1.0f, 0.34f, m);
    float ecx = lerp((float)SCX, dcx, m);
    float ecy = lerp((float)CY, dcy, m);
    drawPlacedEyes(gfx, ecx, ecy, eyeScale, emo, t, LUNI_CYAN, colors.bg);
}

/* ----- 8 thin wrappers (RenderFn has no context pointer) --------------- */
#define MOON_WRAP(NAME, P, R, BR, DUR, EYE, PATH, EXTRAS)                       \
    static void NAME(GfxEngine& gfx, float t, const ColorContext& colors) {     \
        static const MoonPhaseCfg cfg = {P, R, BR, DUR, EYE, PATH, EXTRAS};     \
        renderMoonStory(gfx, t, colors, cfg);                                   \
    }

MOON_WRAP(render_moon_new,    0.00f, 60.0f, 0.010f, 26000, eye_new,    path_new,    extras_new)
MOON_WRAP(render_moon_waxcre, 0.10f, 62.0f, 0.030f, 22000, eye_waxcre, path_waxcre, extras_waxcre)
MOON_WRAP(render_moon_firstq, 0.25f, 64.0f, 0.015f, 22000, eye_firstq, path_firstq, extras_firstq)
MOON_WRAP(render_moon_waxgib, 0.40f, 68.0f, 0.040f, 22000, eye_waxgib, path_waxgib, extras_waxgib)
MOON_WRAP(render_moon_full,   0.50f, 76.0f, 0.020f, 30000, eye_full,   nullptr,     extras_full)
MOON_WRAP(render_moon_wangib, 0.60f, 68.0f, 0.018f, 24000, eye_wangib, path_wangib, extras_wangib)
MOON_WRAP(render_moon_lastq,  0.75f, 64.0f, 0.012f, 22000, eye_lastq,  nullptr,     extras_lastq)
MOON_WRAP(render_moon_wancre, 0.90f, 60.0f, 0.010f, 26000, eye_wancre, path_wancre, extras_wancre)

/* ============================================================
   Variant table + category. Order MUST match SceneManager's lunar-day
   index mapping (canonical Sóc→Rằm→Sóc, == ui_design MOON_SCENE_IDS).
============================================================ */
const VariantDef MOON_VARIANTS[] = {
    {"moon-new",     "New / Soc",          26000, TONE_PURPLE, render_moon_new},
    {"moon-wax-cre", "Waxing crescent",    22000, TONE_CYAN,   render_moon_waxcre},
    {"moon-first-q", "First quarter",      22000, TONE_CYAN,   render_moon_firstq},
    {"moon-wax-gib", "Waxing gibbous",     22000, TONE_WARM,   render_moon_waxgib},
    {"moon-full",    "Full / Ram",         30000, TONE_WARM,   render_moon_full},
    {"moon-wan-gib", "Waning gibbous",     24000, TONE_WARM,   render_moon_wangib},
    {"moon-last-q",  "Last quarter",       22000, TONE_BLUE,   render_moon_lastq},
    {"moon-wan-cre", "Waning crescent",    26000, TONE_PURPLE, render_moon_wancre},
};

extern const CategoryDef CAT_MOON = {
    "moon", "Moon", ContentKind::SCENE, TONE_PURPLE,
    MOON_VARIANTS, (uint8_t)(sizeof(MOON_VARIANTS) / sizeof(MOON_VARIANTS[0])), false
};
