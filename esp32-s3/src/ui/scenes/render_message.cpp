#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "display/SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

/* ======================================================================== */
/*  Shared helpers                                                          */
/* ======================================================================== */

/* -- Helper: draw chat bubble (left = outline only, right = filled) ------ */
static void drawBubble(GfxEngine& gfx, int16_t cx, int16_t cy,
                       int16_t w, int16_t h, bool isRight,
                       uint16_t color, uint8_t alpha) {
    int16_t x = (int16_t)(cx - w / 2);
    int16_t y = (int16_t)(cy - h / 2);
    if (isRight) {
        gfx.fillRoundedRect(x, y, w, h, 10, color, alpha);
    } else {
        gfx.strokeRoundedRect(x, y, w, h, 10, color, 2);
    }
}

/* -- Helper: draw typing dots -------------------------------------------- */
static void drawTypingDots(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float t, uint16_t color, uint8_t alpha) {
    for (int i = 0; i < 3; i++) {
        float phase = fmodf(t * 3.0f - (float)i * 0.25f + 1.0f, 1.0f);
        float bounce = sinf(phase * PI) * 5.0f;
        int16_t dx = (int16_t)(cx - 10 + i * 10);
        int16_t dy = (int16_t)((float)cy - bounce);
        gfx.fillCircle(dx, dy, 3, color, alpha);
    }
}

/* -- Helper: draw checkmarks -------------------------------------------- */
static void drawCheckmarks(GfxEngine& gfx, int16_t cx, int16_t cy,
                           float p, uint16_t color) {
    uint8_t op = (uint8_t)(ease::out(p) * 255);
    // First check
    gfx.drawLine((int16_t)(cx - 8), cy,
                 (int16_t)(cx - 3), (int16_t)(cy + 5), color, 2, op);
    gfx.drawLine((int16_t)(cx - 3), (int16_t)(cy + 5),
                 (int16_t)(cx + 4), (int16_t)(cy - 4), color, 2, op);
    // Second check (offset right)
    if (p > 0.4f) {
        uint8_t op2 = (uint8_t)(ease::out((p - 0.4f) / 0.6f) * 255);
        gfx.drawLine((int16_t)(cx - 2), cy,
                     (int16_t)(cx + 3), (int16_t)(cy + 5), color, 2, op2);
        gfx.drawLine((int16_t)(cx + 3), (int16_t)(cy + 5),
                     (int16_t)(cx + 10), (int16_t)(cy - 4), color, 2, op2);
    }
}

/* -- Helper: draw heart icon --------------------------------------------- */
static void drawHeartIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                          float s, uint16_t color, uint8_t alpha) {
    float k = s / 20.0f;
    int16_t r = (int16_t)(6.0f * k);
    int16_t bumpY = (int16_t)(cy - 6.0f * k);
    int16_t tipY  = (int16_t)(cy + 6.0f * k);
    gfx.fillCircle((int16_t)(cx - r), bumpY, r, color, alpha);
    gfx.fillCircle((int16_t)(cx + r), bumpY, r, color, alpha);
    gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                     (int16_t)(cx + 2 * r), bumpY,
                     cx, tipY, color, alpha);
}

/* ======================================================================== */
/*  Variant 1: message-chat  (14s, TONE_CYAN)                              */
/* ======================================================================== */

static const PhaseEntry CHAT_PHASES[] = {
    {"curious",  0.06f},   // Curious Luni
    {"shrink",   0.05f},   // Eyes shrink
    {"theirIn1", 0.08f},   // Their bubble slides in from left
    {"typing1",  0.08f},   // Typing dots
    {"myIn1",    0.08f},   // My bubble from right
    {"theirIn2", 0.08f},   // Their reply
    {"typing2",  0.08f},   // Typing dots
    {"myIn2",    0.08f},   // My reply
    {"send",     0.08f},   // Send animation
    {"delivered",0.10f},   // Delivered checkmarks
    {"grow",     0.07f},   // Eyes grow back
    {"done",     0.16f},   // Satisfied Luni
};
static constexpr uint8_t CHAT_PHASE_COUNT = sizeof(CHAT_PHASES) / sizeof(CHAT_PHASES[0]);

static void render_message_chat(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, CHAT_PHASES, CHAT_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "curious";

    if (phaseIs(ph.name, "curious")) {
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "theirIn1", "typing1")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "myIn1")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIn(ph.name, "theirIn2", "typing2")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIn(ph.name, "myIn2", "send")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "delivered")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Prop: chat bubbles ------------------------------------------- */
    // Bubble positions (stacking downward)
    int16_t baseY = STATUS_H + 36;
    int16_t rowH  = 32;
    // Left bubbles (theirs): cx ~100, Right bubbles (mine): cx ~220

    // Determine how many bubbles to show based on phase
    int showCount = 0;
    bool showTyping = false;
    bool showChecks = false;

    if (phaseIs(ph.name, "theirIn1"))  { showCount = 1; }
    else if (phaseIs(ph.name, "typing1"))  { showCount = 1; showTyping = true; }
    else if (phaseIs(ph.name, "myIn1"))    { showCount = 2; }
    else if (phaseIs(ph.name, "theirIn2")) { showCount = 3; }
    else if (phaseIs(ph.name, "typing2"))  { showCount = 3; showTyping = true; }
    else if (phaseIs(ph.name, "myIn2"))    { showCount = 4; }
    else if (phaseIs(ph.name, "send"))     { showCount = 4; }
    else if (phaseIs(ph.name, "delivered")) { showCount = 4; showChecks = true; }

    // Draw accumulated bubbles
    struct BubbleInfo { int16_t cx; int16_t cy; int16_t w; bool isRight; };
    const BubbleInfo bubbles[] = {
        {100, (int16_t)(baseY),            70, false},  // their 1
        {220, (int16_t)(baseY + rowH),     80, true},   // my 1
        {110, (int16_t)(baseY + rowH * 2), 60, false},  // their 2
        {210, (int16_t)(baseY + rowH * 3), 90, true},   // my 2
    };

    for (int i = 0; i < showCount && i < 4; i++) {
        // The latest bubble slides in
        bool isLatest = (i == showCount - 1);
        uint8_t alpha = 255;
        int16_t bx = bubbles[i].cx;

        if (isLatest) {
            float slideP = ease::out(ph.p);
            if (bubbles[i].isRight) {
                bx = (int16_t)lerp((float)(SCREEN_W + 40), (float)bubbles[i].cx, slideP);
            } else {
                bx = (int16_t)lerp(-40.0f, (float)bubbles[i].cx, slideP);
            }
            alpha = (uint8_t)(slideP * 255);
        }

        drawBubble(gfx, bx, bubbles[i].cy, bubbles[i].w, 22, bubbles[i].isRight,
                   colors.eye, alpha);
    }

    // Typing dots (below the last "their" bubble)
    if (showTyping) {
        int16_t typingY = (showCount <= 1) ? (int16_t)(baseY + rowH) : (int16_t)(baseY + rowH * 3);
        drawTypingDots(gfx, 100, typingY, ph.p, colors.eye, 200);
    }

    // Checkmarks next to last "my" bubble
    if (showChecks) {
        int16_t lastMyY = (int16_t)(baseY + rowH * 3);
        drawCheckmarks(gfx, 260, (int16_t)(lastMyY + 2), ph.p, colors.eye);
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "theirIn1", "typing1", "myIn1", "theirIn2")) {
        drawActLabel(gfx, "CHAT", colors.eye);
    } else if (phaseIn(ph.name, "typing2", "myIn2", "send")) {
        drawActLabel(gfx, "REPLY", colors.eye);
    } else if (phaseIs(ph.name, "delivered")) {
        drawActLabel(gfx, "DELIVERED", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "ALL CAUGHT UP", colors.eye);
    }
}

/* ======================================================================== */
/*  Variant 2: message-incoming  (11s, TONE_CYAN)                          */
/* ======================================================================== */

static const PhaseEntry MSG_IN_PHASES[] = {
    {"idle",    0.07f},   // Steady Luni
    {"ping",    0.08f},   // Ping notification
    {"shrink",  0.06f},   // Eyes shrink
    {"arrive",  0.12f},   // Message bubble flies in
    {"unread",  0.14f},   // Unread badge "1"
    {"read",    0.14f},   // Bubble highlights (read)
    {"react",   0.10f},   // Heart icon react
    {"grow",    0.08f},   // Eyes grow back
    {"done",    0.21f},   // Happy Luni
};
static constexpr uint8_t MSG_IN_PHASE_COUNT = sizeof(MSG_IN_PHASES) / sizeof(MSG_IN_PHASES[0]);

static void render_message_incoming(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, MSG_IN_PHASES, MSG_IN_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "steady";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "steady";
    } else if (phaseIs(ph.name, "ping")) {
        eyeEmo = "surprised";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "curious";
    } else if (phaseIn(ph.name, "arrive", "unread")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "curious";
    } else if (phaseIs(ph.name, "read")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "react")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Prop: message bubble ----------------------------------------- */
    int16_t bubbleCx = SCX - 20, bubbleCy = CY + 8;
    int16_t bw = 100, bh = 36;

    if (phaseIs(ph.name, "ping")) {
        // Notification ping: expanding ring
        float ringP = ease::out(ph.p);
        int16_t rr = (int16_t)(10.0f + ringP * 30.0f);
        uint8_t ringOp = (uint8_t)((1.0f - ringP) * 200);
        gfx.strokeCircle(SCX, CY, rr, colors.eye, 3);
        (void)ringOp;
        // Small dot at center
        gfx.fillCircle(SCX, CY, 5, colors.eye, (uint8_t)((1.0f - ringP) * 255));
    } else if (phaseIs(ph.name, "shrink")) {
        // Nothing extra
    } else if (phaseIs(ph.name, "arrive")) {
        // Bubble flies in from left
        float ep = ease::out(ph.p);
        int16_t fromX = -60;
        bubbleCx = (int16_t)lerp((float)fromX, (float)(SCX - 20), ep);
        uint8_t alpha = (uint8_t)(ep * 255);
        drawBubble(gfx, bubbleCx, bubbleCy, bw, bh, false, colors.eye, alpha);
        // Content lines
        if (ep > 0.5f) {
            uint8_t lOp = (uint8_t)((ep - 0.5f) * 2.0f * 180);
            gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy - 4),
                         (int16_t)(bubbleCx + 30), (int16_t)(bubbleCy - 4), colors.eye, 2, lOp);
            gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy + 4),
                         (int16_t)(bubbleCx + 14), (int16_t)(bubbleCy + 4), colors.eye, 2, lOp);
        }
    } else if (phaseIs(ph.name, "unread")) {
        // Bubble shown with unread badge
        drawBubble(gfx, bubbleCx, bubbleCy, bw, bh, false, colors.eye, 255);
        gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy - 4),
                     (int16_t)(bubbleCx + 30), (int16_t)(bubbleCy - 4), colors.eye, 2);
        gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy + 4),
                     (int16_t)(bubbleCx + 14), (int16_t)(bubbleCy + 4), colors.eye, 2);

        // Badge "1" (pulsing)
        float badgeScale = 1.0f + sinf(ph.p * TAU * 2.0f) * 0.1f;
        int16_t badgeR = (int16_t)(10.0f * badgeScale);
        int16_t badgeX = (int16_t)(bubbleCx + bw / 2 - 4);
        int16_t badgeY = (int16_t)(bubbleCy - bh / 2 - 4);
        gfx.fillCircle(badgeX, badgeY, badgeR, colors.eye);
        gfx.drawText("1", badgeX, (int16_t)(badgeY + 4), COLOR_BG, 1,
                     GfxEngine::TextAlign::CENTER);
    } else if (phaseIs(ph.name, "read")) {
        // Bubble highlights
        float highlightP = ease::out(clamp(ph.p * 2.0f, 0.0f, 1.0f));
        uint8_t fillOp = (uint8_t)(highlightP * 120);
        gfx.fillRoundedRect((int16_t)(bubbleCx - bw / 2), (int16_t)(bubbleCy - bh / 2),
                            bw, bh, 10, colors.eye, fillOp);
        gfx.strokeRoundedRect((int16_t)(bubbleCx - bw / 2), (int16_t)(bubbleCy - bh / 2),
                              bw, bh, 10, colors.eye, 2);
        gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy - 4),
                     (int16_t)(bubbleCx + 30), (int16_t)(bubbleCy - 4), colors.eye, 2);
        gfx.drawLine((int16_t)(bubbleCx - 30), (int16_t)(bubbleCy + 4),
                     (int16_t)(bubbleCx + 14), (int16_t)(bubbleCy + 4), colors.eye, 2);
    } else if (phaseIs(ph.name, "react")) {
        // Bubble stays, heart icon appears
        gfx.fillRoundedRect((int16_t)(bubbleCx - bw / 2), (int16_t)(bubbleCy - bh / 2),
                            bw, bh, 10, colors.eye, 120);
        gfx.strokeRoundedRect((int16_t)(bubbleCx - bw / 2), (int16_t)(bubbleCy - bh / 2),
                              bw, bh, 10, colors.eye, 2);

        // Heart grows in below the bubble
        float heartP = ease::out(ph.p);
        float heartSize = lerp(4.0f, 16.0f, heartP);
        int16_t heartX = (int16_t)(bubbleCx + bw / 2 - 14);
        int16_t heartY = (int16_t)(bubbleCy + bh / 2 + 10);
        drawHeartIcon(gfx, heartX, heartY, heartSize, colors.eye, (uint8_t)(heartP * 255));
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIn(ph.name, "arrive", "unread")) {
        drawActLabel(gfx, "NEW MESSAGE", colors.eye);
    } else if (phaseIs(ph.name, "read")) {
        drawActLabel(gfx, "READ", colors.eye);
    } else if (phaseIs(ph.name, "react")) {
        drawActLabel(gfx, "REACT", colors.eye);
    }
}

/* ======================================================================== */
/*  Variant 3: message-sent  (11s, TONE_ROSE)                              */
/* ======================================================================== */

static const PhaseEntry MSG_SENT_PHASES[] = {
    {"idle",      0.07f},   // Eager Luni
    {"shrink",    0.06f},   // Eyes shrink
    {"typing",    0.18f},   // Typing dots animate
    {"whoosh",    0.10f},   // Bubble flies right
    {"flight",    0.14f},   // Flight path trail
    {"delivered", 0.12f},   // Delivered checkmarks
    {"grow",      0.08f},   // Eyes grow back
    {"done",      0.25f},   // Happy Luni
};
static constexpr uint8_t MSG_SENT_PHASE_COUNT = sizeof(MSG_SENT_PHASES) / sizeof(MSG_SENT_PHASES[0]);

static void render_message_sent(GfxEngine& gfx, float t, const ColorContext& colors) {
    PhaseResult ph = phases(t, MSG_SENT_PHASES, MSG_SENT_PHASE_COUNT);

    /* --- Eye state ---------------------------------------------------- */
    float eyeCx = (float)SCX, eyeCy = (float)CY, eyeScale = 1.0f;
    const char* eyeEmo = "eager";

    if (phaseIs(ph.name, "idle")) {
        eyeEmo = "eager";
    } else if (phaseIs(ph.name, "shrink")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp((float)SCX, 284.0f, ep);
        eyeCy    = lerp((float)CY,  44.0f,  ep);
        eyeScale = lerp(1.0f, 0.30f, ep);
        eyeEmo   = "eager";
    } else if (phaseIs(ph.name, "typing")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "thinking";
    } else if (phaseIn(ph.name, "whoosh", "flight")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "excited";
    } else if (phaseIs(ph.name, "delivered")) {
        eyeCx = 284.0f; eyeCy = 44.0f; eyeScale = 0.30f;
        eyeEmo = "happy";
    } else if (phaseIs(ph.name, "grow")) {
        float ep = ease::inOut(ph.p);
        eyeCx    = lerp(284.0f, (float)SCX, ep);
        eyeCy    = lerp(44.0f,  (float)CY,  ep);
        eyeScale = lerp(0.30f, 1.0f, ep);
        eyeEmo   = "satisfied";
    } else if (phaseIs(ph.name, "done")) {
        eyeEmo = "satisfied";
    }

    /* --- Prop: typing & sending --------------------------------------- */
    int16_t msgCx = SCX, msgCy = CY + 8;

    if (phaseIs(ph.name, "typing")) {
        // Message input area with typing dots
        gfx.strokeRoundedRect((int16_t)(msgCx - 60), (int16_t)(msgCy - 14),
                              120, 28, 12, colors.eye, 2);
        drawTypingDots(gfx, msgCx, msgCy, ph.p, colors.eye, 220);
    } else if (phaseIs(ph.name, "whoosh")) {
        // Bubble flies to the right
        float ep = ease::in(ph.p);
        int16_t bx = (int16_t)lerp((float)msgCx, (float)(SCREEN_W + 40), ep);
        uint8_t alpha = (uint8_t)((1.0f - ep * 0.5f) * 255);
        gfx.fillRoundedRect((int16_t)(bx - 50), (int16_t)(msgCy - 14),
                            100, 28, 12, colors.eye, alpha);

        // Speed lines behind
        for (int i = 0; i < 3; i++) {
            float lp = clamp(ep - (float)i * 0.1f, 0.0f, 1.0f);
            int16_t lx = (int16_t)(bx - 60 - (float)i * 12);
            uint8_t lOp = (uint8_t)((1.0f - lp) * 150);
            gfx.drawLine(lx, (int16_t)(msgCy - 4 + i * 4),
                         (int16_t)(lx + 16), (int16_t)(msgCy - 4 + i * 4),
                         colors.eye, 2, lOp);
        }
    } else if (phaseIs(ph.name, "flight")) {
        // Trail particles moving right
        for (int i = 0; i < 5; i++) {
            float fp = fmodf(ph.p * 2.0f + (float)i * 0.2f, 1.0f);
            int16_t fx = (int16_t)(40.0f + fp * (float)(SCREEN_W - 80));
            int16_t fy = (int16_t)((float)msgCy + sinf(fp * PI * 2.0f) * 8.0f);
            uint8_t op = (uint8_t)((1.0f - fp) * 180);
            gfx.fillCircle(fx, fy, (int16_t)(3.0f - fp * 2.0f), colors.eye, op);
        }

        // Small bubble arriving at right side
        float arriveP = ease::out(clamp((ph.p - 0.5f) * 2.0f, 0.0f, 1.0f));
        if (arriveP > 0.0f) {
            int16_t destX = (int16_t)(SCREEN_W - 70);
            uint8_t destOp = (uint8_t)(arriveP * 255);
            gfx.fillRoundedRect((int16_t)(destX - 40), (int16_t)(msgCy - 12),
                                80, 24, 10, colors.eye, destOp);
        }
    } else if (phaseIs(ph.name, "delivered")) {
        // Bubble at destination with checkmarks
        int16_t destX = (int16_t)(SCREEN_W - 70);
        gfx.fillRoundedRect((int16_t)(destX - 40), (int16_t)(msgCy - 12),
                            80, 24, 10, colors.eye, 255);
        drawCheckmarks(gfx, (int16_t)(destX + 46), (int16_t)(msgCy + 8), ph.p, colors.eye);
    }

    /* --- Eyes --------------------------------------------------------- */
    drawPlacedEyes(gfx, eyeCx, eyeCy, eyeScale, eyeEmo, t,
                   TONE_LUT[TONE_CYAN], colors.bg);

    /* --- Label -------------------------------------------------------- */
    if (phaseIs(ph.name, "typing")) {
        drawActLabel(gfx, "TYPING", colors.eye);
    } else if (phaseIn(ph.name, "whoosh", "flight")) {
        drawActLabel(gfx, "SENDING", colors.eye);
    } else if (phaseIs(ph.name, "delivered")) {
        drawActLabel(gfx, "DELIVERED", colors.eye);
    } else if (phaseIs(ph.name, "done")) {
        drawActLabel(gfx, "SENT!", colors.eye);
    }
}

/* ======================================================================== */
/*  Registration                                                            */
/* ======================================================================== */

const VariantDef MESSAGE_VARIANTS[] = {
    {"message-chat",     "Chat",     14000, TONE_NONE, render_message_chat},
    {"message-incoming", "Incoming", 11000, TONE_NONE, render_message_incoming},
    {"message-sent",     "Sent",     11000, TONE_ROSE, render_message_sent},
};

extern const CategoryDef CAT_MESSAGE = {
    "message", "Message", ContentKind::SCENE, TONE_CYAN,
    MESSAGE_VARIANTS, sizeof(MESSAGE_VARIANTS) / sizeof(MESSAGE_VARIANTS[0]), false
};
