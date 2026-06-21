#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry CAMERA_PHASES[] = {
    {"idle",    0.06f},   // Curious Luni, centered
    {"camIn",   0.08f},   // Camera slides in from left
    {"shrink",  0.05f},   // Eyes shrink to corner
    {"focus",   0.14f},   // Focus brackets converge
    {"flash",   0.06f},   // White screen flash
    {"develop", 0.20f},   // Photo develops (fade in)
    {"show",    0.12f},   // Photo shown on screen
    {"camOut",  0.06f},   // Camera exits to left
    {"grow",    0.07f},   // Eyes grow back to center
    {"done",    0.16f},   // Happy Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(CAMERA_PHASES) / sizeof(CAMERA_PHASES[0]);

/* -- Helper: draw camera body -------------------------------------------- */
static void drawCamera(GfxEngine& gfx, int16_t cx, int16_t cy,
                       uint16_t color, uint8_t alpha) {
    // Camera body
    gfx.fillRoundedRect((int16_t)(cx - 34), (int16_t)(cy - 20), 68, 44, 6, color, alpha);
    // Lens (outer ring)
    gfx.fillCircle(cx, (int16_t)(cy + 2), 14, COLOR_BG, alpha);
    // Lens (inner)
    gfx.fillCircle(cx, (int16_t)(cy + 2), 8, color, alpha);
    // Flash bump on top-right
    gfx.fillRoundedRect((int16_t)(cx + 16), (int16_t)(cy - 28), 14, 8, 2, color, alpha);
    // Viewfinder bump
    gfx.fillRoundedRect((int16_t)(cx - 8), (int16_t)(cy - 28), 16, 8, 2, color, alpha);
}

/* -- Helper: draw focus brackets (4 L-corners) --------------------------- */
static void drawFocusBrackets(GfxEngine& gfx, int16_t cx, int16_t cy,
                              float size, uint16_t color, uint8_t alpha) {
    int16_t half = (int16_t)(size / 2.0f);
    int16_t arm = 14;

    // Top-left
    gfx.drawLine((int16_t)(cx - half), (int16_t)(cy - half),
                 (int16_t)(cx - half + arm), (int16_t)(cy - half), color, 3, alpha);
    gfx.drawLine((int16_t)(cx - half), (int16_t)(cy - half),
                 (int16_t)(cx - half), (int16_t)(cy - half + arm), color, 3, alpha);
    // Top-right
    gfx.drawLine((int16_t)(cx + half), (int16_t)(cy - half),
                 (int16_t)(cx + half - arm), (int16_t)(cy - half), color, 3, alpha);
    gfx.drawLine((int16_t)(cx + half), (int16_t)(cy - half),
                 (int16_t)(cx + half), (int16_t)(cy - half + arm), color, 3, alpha);
    // Bottom-left
    gfx.drawLine((int16_t)(cx - half), (int16_t)(cy + half),
                 (int16_t)(cx - half + arm), (int16_t)(cy + half), color, 3, alpha);
    gfx.drawLine((int16_t)(cx - half), (int16_t)(cy + half),
                 (int16_t)(cx - half), (int16_t)(cy + half - arm), color, 3, alpha);
    // Bottom-right
    gfx.drawLine((int16_t)(cx + half), (int16_t)(cy + half),
                 (int16_t)(cx + half - arm), (int16_t)(cy + half), color, 3, alpha);
    gfx.drawLine((int16_t)(cx + half), (int16_t)(cy + half),
                 (int16_t)(cx + half), (int16_t)(cy + half - arm), color, 3, alpha);
}

/* -- Helper: draw photo frame -------------------------------------------- */
static void drawPhotoFrame(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float opacity, uint16_t color) {
    uint8_t op = (uint8_t)(opacity * 255);
    // Outer frame
    gfx.strokeRoundedRect((int16_t)(cx - 44), (int16_t)(cy - 34), 88, 68, 4, color, 3);
    // Inner "photo" area (filled)
    gfx.fillRoundedRect((int16_t)(cx - 38), (int16_t)(cy - 28), 76, 56, 2, color, op);
    // Simple landscape inside: horizon line and circle "sun"
    if (opacity > 0.5f) {
        uint8_t innerOp = (uint8_t)((opacity - 0.5f) * 2.0f * 200);
        gfx.drawLine((int16_t)(cx - 30), (int16_t)(cy + 4),
                     (int16_t)(cx + 30), (int16_t)(cy + 4), COLOR_BG, 2, innerOp);
        gfx.fillCircle((int16_t)(cx + 14), (int16_t)(cy - 10), 8, COLOR_BG, innerOp);
    }
}

/* -- Main render --------------------------------------------------------- */
static void render_camera_snap(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, CAMERA_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "camIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIs(ph.name, "focus")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "flash")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "surprised";
    } else if (phaseIn(ph.name, "develop", "show")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "camOut")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "happy";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "happy";
    }

    /* --- Prop: camera & photo ----------------------------------------- */
    int16_t camCx = SCX - 20, camCy = CY - 10;
    int16_t photoCx = SCX, photoCy = CY + 8;

    if (phaseIs(ph.name, "camIn")) {
        // Slide in from left
        float ep = ease::out(ph.p);
        int16_t fromX = -60;
        camCx = (int16_t)lerp((float)fromX, (float)(SCX - 20), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawCamera(gfx, camCx, camCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawCamera(gfx, camCx, camCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "focus")) {
        // Camera stays, focus brackets converge
        drawCamera(gfx, camCx, camCy, colors.eye, 255);
        float bracketSize = lerp(120.0f, 50.0f, ease::out(ph.p));
        drawFocusBrackets(gfx, photoCx, photoCy, bracketSize, colors.eye, 255);
        // Center dot
        if (ph.p > 0.5f) {
            gfx.fillCircle(photoCx, photoCy, 3, colors.eye);
        }
    } else if (phaseIs(ph.name, "flash")) {
        // White screen flash effect
        float flashP = 1.0f - ph.p;
        uint8_t flashAlpha = (uint8_t)(flashP * 220);
        gfx.fillRect(0, STATUS_H, SCREEN_W, SCREEN_H - STATUS_H, colors.eye, flashAlpha);
    } else if (phaseIs(ph.name, "develop")) {
        // Photo develops from center (fade in)
        float devP = ease::out(ph.p);
        drawPhotoFrame(gfx, photoCx, photoCy, devP, colors.eye);
    } else if (phaseIs(ph.name, "show")) {
        // Photo fully shown
        drawPhotoFrame(gfx, photoCx, photoCy, 1.0f, colors.eye);

        // Gentle sparkle at corners
        for (int i = 0; i < 4; i++) {
            float a = (float)i * (TAU / 4.0f) + ph.p * TAU;
            float r = 50.0f;
            int16_t sx = (int16_t)(photoCx + cosf(a) * r);
            int16_t sy = (int16_t)(photoCy + sinf(a) * r * 0.7f);
            float sp = fmodf(ph.p * 2.0f + (float)i * 0.25f, 1.0f);
            uint8_t op = (uint8_t)((1.0f - sp) * 180);
            gfx.fillCircle(sx, sy, 3, colors.eye, op);
        }
    } else if (phaseIs(ph.name, "camOut")) {
        // Camera exits to left
        float ep = ease::in(ph.p);
        camCx = (int16_t)lerp((float)(SCX - 20), -70.0f, ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawCamera(gfx, camCx, camCy, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIs(ph.name, "focus")) {
        drawActLabel(gfx, "FOCUS", colors.eye);
    } else if (phaseIs(ph.name, "flash")) {
        drawActLabel(gfx, "SNAP!", colors.eye);
    } else if (phaseIn(ph.name, "develop", "show")) {
        drawActLabel(gfx, "PHOTO", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "NICE SHOT", colors.eye);
    }
}

const VariantDef CAMERA_VARIANTS[] = {
    {"camera-snap", "Snap & save", 14000, TONE_NONE, render_camera_snap},
};

extern const CategoryDef CAT_CAMERA = {
    "camera", "Camera", ContentKind::SCENE, TONE_WHITE,
    CAMERA_VARIANTS, sizeof(CAMERA_VARIANTS) / sizeof(CAMERA_VARIANTS[0]), false
};
