#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* -- Phase table --------------------------------------------------------- */
static const PhaseEntry READING_PHASES[] = {
    {"idle",     0.06f},   // Luni curious, centered
    {"bookIn",   0.07f},   // Open book slides in from left
    {"shrink",   0.05f},   // Eyes shrink to top-right corner
    {"read1",    0.18f},   // Text lines appear on left page
    {"turn",     0.06f},   // Page turn: lines shift
    {"read2",    0.18f},   // More text lines on right page
    {"bookmark", 0.08f},   // Bookmark placed on page
    {"bookOut",  0.07f},   // Book exits to right
    {"grow",     0.07f},   // Eyes grow back to center
    {"done",     0.18f},   // Satisfied Luni
};
static constexpr uint8_t PHASE_COUNT = sizeof(READING_PHASES) / sizeof(READING_PHASES[0]);

/* -- Helper: draw open book ---------------------------------------------- */
static void drawOpenBook(GfxEngine& gfx, int16_t cx, int16_t cy,
                         uint16_t color, uint8_t alpha) {
    // Left page
    gfx.fillRoundedRect((int16_t)(cx - 64), (int16_t)(cy - 40), 62, 80, 3, color, (uint8_t)(alpha * 0.3f));
    gfx.strokeRoundedRect((int16_t)(cx - 64), (int16_t)(cy - 40), 62, 80, 3, color, 2);

    // Right page
    gfx.fillRoundedRect((int16_t)(cx + 2), (int16_t)(cy - 40), 62, 80, 3, color, (uint8_t)(alpha * 0.3f));
    gfx.strokeRoundedRect((int16_t)(cx + 2), (int16_t)(cy - 40), 62, 80, 3, color, 2);

    // Spine
    gfx.drawLine(cx, (int16_t)(cy - 40), cx, (int16_t)(cy + 40), color, 2, alpha);
}

/* -- Helper: draw text lines on a page ----------------------------------- */
static void drawTextLines(GfxEngine& gfx, int16_t pageX, int16_t pageY,
                          int lineCount, float reveal, uint16_t color) {
    for (int i = 0; i < lineCount; i++) {
        float lineP = clamp((reveal - (float)i * 0.12f) / 0.3f, 0.0f, 1.0f);
        if (lineP <= 0.0f) continue;

        int16_t ly = (int16_t)(pageY - 28 + i * 12);
        int16_t lw = (int16_t)((38 + ((i * 7) % 12)) * lineP);
        uint8_t op = (uint8_t)(lineP * 200);
        gfx.drawLine((int16_t)(pageX + 8), ly,
                     (int16_t)(pageX + 8 + lw), ly, color, 2, op);
    }
}

/* -- Helper: draw bookmark ----------------------------------------------- */
static void drawBookmark(GfxEngine& gfx, int16_t cx, int16_t cy,
                         float insertP, uint16_t color, uint8_t alpha) {
    int16_t bmX = (int16_t)(cx + 36);
    int16_t bmTop = (int16_t)(cy - 40 - 8);
    int16_t bmH = (int16_t)(20.0f + insertP * 18.0f);
    gfx.fillRect((int16_t)(bmX - 3), bmTop, 6, bmH, color, alpha);
    // Small triangle notch at bottom
    gfx.fillTriangle((int16_t)(bmX - 3), (int16_t)(bmTop + bmH),
                     (int16_t)(bmX + 3), (int16_t)(bmTop + bmH),
                     bmX, (int16_t)(bmTop + bmH + 5), color, alpha);
}

/* -- Main render --------------------------------------------------------- */
static void render_reading(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, READING_PHASES, PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "bookIn")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "read1", "turn")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "read2")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIs(ph.name, "bookmark")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "satisfied";
    } else if (phaseIs(ph.name, "bookOut")) {
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

    /* --- Prop: book --------------------------------------------------- */
    int16_t bookCx = SCX, bookCy = CY + 10;

    if (phaseIs(ph.name, "bookIn")) {
        // Slide in from left
        float ep = ease::out(ph.p);
        bookCx = (int16_t)lerp(-80.0f, (float)SCX, ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, alpha);
    } else if (phaseIs(ph.name, "shrink")) {
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, 255);
    } else if (phaseIs(ph.name, "read1")) {
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, 255);
        // Text lines appear on left page
        drawTextLines(gfx, (int16_t)(bookCx - 64), bookCy, 5, ph.p * 2.0f, colors.eye);
    } else if (phaseIs(ph.name, "turn")) {
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, 255);
        // Old lines fade, shift effect
        uint8_t fadeOp = (uint8_t)((1.0f - ph.p) * 200);
        drawTextLines(gfx, (int16_t)(bookCx - 64), bookCy, 5, 1.0f, colors.eye);
        // Turning page overlay
        float turnOffset = ph.p * 60.0f;
        gfx.fillRect((int16_t)(bookCx - 64 + (int16_t)turnOffset), (int16_t)(bookCy - 40),
                     (int16_t)(62 - turnOffset), 80, colors.eye, (uint8_t)(fadeOp * 0.15f));
    } else if (phaseIs(ph.name, "read2")) {
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, 255);
        // Text lines on left page (already there)
        drawTextLines(gfx, (int16_t)(bookCx - 64), bookCy, 5, 1.0f, colors.eye);
        // Text lines appear on right page
        drawTextLines(gfx, (int16_t)(bookCx + 2), bookCy, 5, ph.p * 2.0f, colors.eye);
    } else if (phaseIs(ph.name, "bookmark")) {
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, 255);
        drawTextLines(gfx, (int16_t)(bookCx - 64), bookCy, 5, 1.0f, colors.eye);
        drawTextLines(gfx, (int16_t)(bookCx + 2), bookCy, 5, 1.0f, colors.eye);
        // Bookmark slides in from top
        drawBookmark(gfx, bookCx, bookCy, ease::out(ph.p), colors.eye, 255);
    } else if (phaseIs(ph.name, "bookOut")) {
        // Book exits to right
        float ep = ease::in(ph.p);
        bookCx = (int16_t)lerp((float)SCX, (float)(SCREEN_W + 80), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep) * 255);
        drawOpenBook(gfx, bookCx, bookCy, colors.eye, alpha);
        drawBookmark(gfx, bookCx, bookCy, 1.0f, colors.eye, alpha);
    }

    /* --- Eyes (always on top) ----------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_WARM], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "read1", "turn", "read2")) {
        drawActLabel(gfx, "READING", colors.eye);
    } else if (phaseIs(ph.name, "bookmark")) {
        drawActLabel(gfx, "BOOKMARKED", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ENJOYED", colors.eye);
    }
}

const VariantDef READING_VARIANTS[] = {
    {"reading-main", "Reading", 14000, TONE_NONE, render_reading},
};

extern const CategoryDef CAT_READING = {
    "reading", "Reading", ContentKind::SCENE, TONE_WARM,
    READING_VARIANTS, sizeof(READING_VARIANTS) / sizeof(READING_VARIANTS[0]), false
};
