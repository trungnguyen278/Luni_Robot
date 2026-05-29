#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry SHOME_PHASES[] = {
    {"idle",     0.06f},   // Luni steady, centered
    {"houseIn",  0.07f},   // House outline appears (fade in)
    {"shrink",   0.05f},   // Eyes shrink to top-right corner
    {"light",    0.14f},   // Lightbulb toggles on (glow)
    {"thermo",   0.14f},   // Thermostat bar fills up
    {"check",    0.12f},   // Checkmark drawn
    {"houseOut", 0.07f},   // House fades out
    {"grow",     0.07f},   // Eyes grow back to center
    {"done",     0.28f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(SHOME_PHASES) / sizeof(SHOME_PHASES[0]);

/* -- Helper: draw house outline ------------------------------------------ */
static void drawHouse(GfxEngine& gfx, int16_t cx, int16_t cy,
                      uint16_t color, uint8_t alpha) {
    int16_t bodyW = 100, bodyH = 60;
    int16_t bodyTop = (int16_t)(cy - 10);
    int16_t bodyBot = (int16_t)(bodyTop + bodyH);

    // Body (stroked rect)
    gfx.strokeRoundedRect((int16_t)(cx - bodyW / 2), bodyTop,
                          bodyW, bodyH, 3, color, 3);

    // Triangle roof
    gfx.drawLine((int16_t)(cx - bodyW / 2 - 8), bodyTop,
                 cx, (int16_t)(cy - 50), color, 3, alpha);
    gfx.drawLine(cx, (int16_t)(cy - 50),
                 (int16_t)(cx + bodyW / 2 + 8), bodyTop, color, 3, alpha);
    // Roof base line
    gfx.drawLine((int16_t)(cx - bodyW / 2 - 8), bodyTop,
                 (int16_t)(cx + bodyW / 2 + 8), bodyTop, color, 2, alpha);

    // Door
    gfx.strokeRect((int16_t)(cx - 8), (int16_t)(bodyBot - 28),
                   16, 28, color, 2);
    // Door knob
    gfx.fillCircle((int16_t)(cx + 4), (int16_t)(bodyBot - 12), 2, color, alpha);
}

/* -- Helper: draw lightbulb ---------------------------------------------- */
static void drawBulb(GfxEngine& gfx, int16_t cx, int16_t cy,
                     float onAmount, uint16_t color, uint8_t alpha) {
    // Bulb circle
    int16_t r = 12;
    uint8_t bulbAlpha = (uint8_t)(alpha * (0.3f + onAmount * 0.7f));
    gfx.fillCircle(cx, cy, r, color, bulbAlpha);

    // Glow ring when on
    if (onAmount > 0.1f) {
        uint8_t glowAlpha = (uint8_t)(onAmount * 100);
        gfx.strokeCircle(cx, cy, (int16_t)(r + 4), color, 2);
        gfx.strokeCircle(cx, cy, (int16_t)(r + 8), color, 1);
        (void)glowAlpha;
    }

    // Small rect stem below bulb
    gfx.fillRect((int16_t)(cx - 4), (int16_t)(cy + r), 8, 6, color, alpha);
    // Base lines
    gfx.drawLine((int16_t)(cx - 5), (int16_t)(cy + r + 6),
                 (int16_t)(cx + 5), (int16_t)(cy + r + 6), color, 2, alpha);
}

/* -- Helper: draw thermostat --------------------------------------------- */
static void drawThermostat(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float fillLevel, uint16_t color, uint8_t alpha) {
    int16_t barW = 14, barH = 50;
    int16_t barX = (int16_t)(cx - barW / 2);
    int16_t barY = (int16_t)(cy - barH / 2);

    // Outer frame
    gfx.strokeRoundedRect(barX, barY, barW, barH, 4, color, 2);

    // Fill level (from bottom up)
    int16_t fillH = (int16_t)(barH * fillLevel);
    if (fillH > 2) {
        gfx.fillRoundedRect(barX + 2, (int16_t)(barY + barH - fillH),
                            barW - 4, fillH, 2, color, alpha);
    }

    // Temperature tick marks on side
    for (int i = 0; i < 4; i++) {
        int16_t tickY = (int16_t)(barY + 8 + i * 11);
        gfx.drawLine((int16_t)(barX - 4), tickY,
                     (int16_t)(barX - 1), tickY, color, 1, (uint8_t)(alpha * 0.6f));
    }
}

/* -- Helper: draw checkmark ---------------------------------------------- */
static void drawCheck(GfxEngine& gfx, int16_t cx, int16_t cy,
                      float progress, uint16_t color, uint8_t alpha) {
    int16_t x0 = (int16_t)(cx - 14);
    int16_t y0 = cy;
    int16_t x1 = (int16_t)(cx - 4);
    int16_t y1 = (int16_t)(cy + 12);
    int16_t x2 = (int16_t)(cx + 16);
    int16_t y2 = (int16_t)(cy - 12);

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

/* -- Main render --------------------------------------------------------- */
static void render_smarthome(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, SHOME_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "houseIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIs(ph.name, "light")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "thermo")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "check")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "houseOut")) {
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

    /* --- Prop: house + devices ---------------------------------------- */
    int16_t houseCx = SCX, houseCy = CY + 10;
    int16_t bulbCx = (int16_t)(houseCx - 28), bulbCy = (int16_t)(houseCy + 6);
    int16_t thermCx = (int16_t)(houseCx + 30), thermCy = (int16_t)(houseCy + 16);

    if (phaseIs(ph.name, "houseIn")) {
        // Fade in
        uint8_t alpha = (uint8_t)(ease::out(ph.p) * 255);
        drawHouse(gfx, houseCx, houseCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawHouse(gfx, houseCx, houseCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "light")) {
        drawHouse(gfx, houseCx, houseCy, colors.eye, 255);
        // Lightbulb toggles on
        float onP = ease::out(clamp(ph.p * 2.0f, 0.0f, 1.0f));
        // Flicker effect near the start
        float flicker = ph.p < 0.3f ? sinf(ph.p * TAU * 8.0f) * 0.3f : 0.0f;
        drawBulb(gfx, bulbCx, bulbCy, onP + flicker, colors.eye, 255);
    } else if (phaseIs(ph.name, "thermo")) {
        drawHouse(gfx, houseCx, houseCy, colors.eye, 255);
        // Bulb stays on
        drawBulb(gfx, bulbCx, bulbCy, 1.0f, colors.eye, 255);
        // Thermostat fills up
        float fillP = ease::inOut(ph.p);
        drawThermostat(gfx, thermCx, thermCy, fillP * 0.75f, colors.eye, 255);
    } else if (phaseIs(ph.name, "check")) {
        drawHouse(gfx, houseCx, houseCy, colors.eye, 255);
        drawBulb(gfx, bulbCx, bulbCy, 1.0f, colors.eye, 255);
        drawThermostat(gfx, thermCx, thermCy, 0.75f, colors.eye, 255);
        // Checkmark draws in center
        float checkP = clamp(ph.p / 0.7f, 0.0f, 1.0f);
        drawCheck(gfx, houseCx, (int16_t)(houseCy - 30), checkP, colors.eye, 255);
    } else if (phaseIs(ph.name, "houseOut")) {
        // Fade out
        float ep = ease::in(ph.p);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawHouse(gfx, houseCx, houseCy, colors.eye, alpha);
        drawBulb(gfx, bulbCx, bulbCy, 1.0f - ep, colors.eye, alpha);
        drawThermostat(gfx, thermCx, thermCy, 0.75f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_GREEN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIs(ph.name, "light")) {
        drawActLabel(gfx, "LIGHTS", colors.eye);
    } else if (phaseIs(ph.name, "thermo")) {
        drawActLabel(gfx, "CLIMATE", colors.eye);
    } else if (phaseIs(ph.name, "check")) {
        drawActLabel(gfx, "ALL SET", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "SMART HOME", colors.eye);
    }
}

const VariantDef SMARTHOME_VARIANTS[] = {
    {"smarthome-main", "Smart Home", 14000, TONE_NONE, render_smarthome},
};

extern const CategoryDef CAT_SMARTHOME = {
    "smarthome", "Smart Home", ContentKind::SCENE, TONE_GREEN,
    SMARTHOME_VARIANTS, sizeof(SMARTHOME_VARIANTS) / sizeof(SMARTHOME_VARIANTS[0]), false
};
