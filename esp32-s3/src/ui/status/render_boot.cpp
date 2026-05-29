#include "ui/VariantRegistry.hpp"
#include "ui/SceneManager.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include "config/BootSubsystems.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace geom;
using namespace math;

// ─── boot-poweron ──────────────────────────────────────────────────
// Dot → horizontal slit → splits into two eye blocks → settles to
// default eye proportions.  Matches JSX boot-poweron exactly.
static void render_boot_poweron(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint16_t c = colors.eye;

    if (t < 0.2f) {
        float p = ease::out(t / 0.2f);
        int16_t r = (int16_t)lerp(1.0f, 8.0f, p);
        gfx.fillRoundedRect(SCX - r, CY - r, r * 2, r * 2, r, c);
    } else if (t < 0.5f) {
        float p = ease::inOut((t - 0.2f) / 0.3f);
        int16_t bw = (int16_t)lerp(16.0f, (float)(GAP + EYE_W), p);
        int16_t bh = (int16_t)lerp(16.0f, 14.0f, p);
        int16_t rx = (int16_t)fminf((float)EYE_RX, bh / 2.0f);
        gfx.fillRoundedRect(SCX - bw / 2, CY - bh / 2, bw, bh, rx, c);
    } else if (t < 0.8f) {
        float p = ease::inOut((t - 0.5f) / 0.3f);
        int16_t bh = (int16_t)lerp(14.0f, (float)EYE_H * 0.6f, p);
        int16_t rx = (int16_t)fminf((float)EYE_RX, bh / 2.0f);
        int16_t lcx = (int16_t)lerp((float)SCX, (float)LX, p);
        int16_t rcx = (int16_t)lerp((float)SCX, (float)RX, p);
        int16_t hw = (int16_t)lerp((float)(GAP + EYE_W) / 2.0f,
                                   (float)EYE_W / 2.0f, p);
        gfx.fillRoundedRect(lcx - hw, CY - bh / 2, hw * 2, bh, rx, c);
        gfx.fillRoundedRect(rcx - hw, CY - bh / 2, hw * 2, bh, rx, c);
    } else {
        float p = ease::inOut((t - 0.8f) / 0.2f);
        int16_t bh = (int16_t)lerp((float)EYE_H * 0.6f, (float)EYE_H, p);
        gfx.drawEye(LX, CY, EYE_W, bh, EYE_RX, 0, c);
        gfx.drawEye(RX, CY, EYE_W, bh, EYE_RX, 0, c);
    }
}

// ─── boot-credits ──────────────────────────────────────────────────
// NTT monogram fades in → sweep arc → name typewriter → role → tagline.
// Everything uses colors.eye (cyan) — no accent color switching.
static void render_boot_credits(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint16_t c = colors.eye;

    float monoIn   = clamp(t / 0.30f, 0.0f, 1.0f);
    float sweep    = clamp((t - 0.20f) / 0.30f, 0.0f, 1.0f);
    float nameP    = clamp((t - 0.30f) / 0.35f, 0.0f, 1.0f);
    float roleP    = clamp((t - 0.55f) / 0.30f, 0.0f, 1.0f);
    float taglineP = clamp((t - 0.65f) / 0.30f, 0.0f, 1.0f);

    int16_t mY = STATUS_H + 56;
    uint8_t monoOp = (uint8_t)(ease::out(monoIn) * 255.0f);

    // NTT monogram — frame + text, both fade in together
    if (monoIn > 0.01f) {
        gfx.pushAlpha(monoOp);
        gfx.strokeRoundedRect(SCX - 28, mY - 28, 56, 56, 12, c, 2);
        gfx.drawText("NTT", SCX, mY - 5, c, 2,
                     GfxEngine::TextAlign::CENTER);
        gfx.popAlpha();
    }

    // Sweep arc around monogram
    if (sweep > 0.02f) {
        float sweepAngle = sweep * TAU;
        float startA = -PI / 2.0f;
        gfx.drawArc(SCX, mY, 36, startA, startA + sweepAngle, c, 2);
    }

    // Name typewriter
    if (nameP > 0.0f) {
        static const char NAME[] = "NGUYEN THANH TRUNG";
        int total = (int)strlen(NAME);
        int len = (int)(nameP * total + 0.0001f);
        if (len > total) len = total;
        char buf[20];
        memcpy(buf, NAME, len);
        buf[len] = '\0';
        gfx.drawText(buf, SCX, mY + 56, c, 1,
                     GfxEngine::TextAlign::CENTER);
    }

    // Role line
    if (roleP > 0.0f) {
        uint8_t rOp = (uint8_t)(ease::out(roleP) * 230.0f);
        gfx.drawText("design \xB7 firmware \xB7 hw", SCX, mY + 74, c, 1,
                     GfxEngine::TextAlign::CENTER, rOp);
    }

    // Bottom tagline
    if (taglineP > 0.0f) {
        uint8_t tOp = (uint8_t)(ease::out(taglineP) * 153.0f);
        gfx.drawText("one-person build", SCX, SCREEN_H - 14, c, 1,
                     GfxEngine::TextAlign::CENTER, tOp);
    }
}

// ─── boot-checks-personal ──────────────────────────────────────────
// Self-test checklist matching the actual init subsystems.
// Items appear one by one; spinner → OK for each.
static void render_boot_checks_personal(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint16_t c = colors.eye;

    const auto& sd = SceneManager::instance().getSceneData();

    gfx.drawText("SYSTEM CHECK", SCX, STATUS_H + 22, c, 1,
                 GfxEngine::TextAlign::CENTER, 217);
    gfx.drawLine(48, STATUS_H + 30, SCREEN_W - 48, STATUS_H + 30, c, 1, 89);

    for (int i = 0; i < BOOT_SUBSYSTEM_COUNT; i++) {
        float start = i * 0.15f;
        float showT = clamp((t - start) / 0.1f, 0.0f, 1.0f);
        if (showT <= 0.0f) continue;

        float okT = clamp((t - start - 0.1f) / 0.06f, 0.0f, 1.0f);
        int16_t y = STATUS_H + 50 + i * 26;
        uint8_t rowOp = (uint8_t)(ease::out(showT) * 255.0f);

        gfx.drawText(BOOT_SUBSYSTEMS[i].label, 48, y, c, 1,
                     GfxEngine::TextAlign::LEFT, rowOp);

        gfx.drawDashedLine(48 + BOOT_SUBSYSTEMS[i].labelW + 4, y + 4,
                           SCREEN_W - 80, y + 4, c, 1, 2, 3);

        bool passed = sd.boot_check_results[i] == BOOT_OK;
        if (okT < 1.0f) {
            float ang = t * TAU * 4.0f;
            gfx.drawArc(SCREEN_W - 60, y + 2, 8, ang, ang + 2.0f,
                        c, 2, rowOp);
        } else {
            const char* status = passed ? "OK" : "ERR";
            gfx.drawText(status, SCREEN_W - 56, y, c, 1,
                         GfxEngine::TextAlign::LEFT, rowOp);
        }
    }

    gfx.drawText("v2.0", SCX, SCREEN_H - 8, c, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// ─── boot-ready-personal ───────────────────────────────────────────
// NTT mark at top + progress bar 0→100% + READY stamp.
static void render_boot_ready_personal(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint16_t c = colors.eye;

    float fill = clamp(t / 0.7f, 0.0f, 1.0f);
    float stampT = clamp((t - 0.75f) / 0.15f, 0.0f, 1.0f);

    // Small NTT monogram at top
    int16_t markY = STATUS_H + 38;
    gfx.strokeRoundedRect(SCX - 12, markY - 12, 24, 24, 5, c, 1);
    gfx.drawText("NTT", SCX, markY - 1, c, 1,
                 GfxEngine::TextAlign::CENTER, 242);

    // Progress bar
    int16_t barW = SCREEN_W - 96;
    int16_t barX = (SCREEN_W - barW) / 2;
    int16_t barY = SCY + 8;

    gfx.strokeRoundedRect(barX, barY, barW, 10, 3, c, 1);
    int16_t fw = (int16_t)((float)(barW - 4) * fill);
    if (fw > 0) {
        gfx.fillRoundedRect(barX + 2, barY + 2, fw, 6, 2, c);
    }

    // Percentage
    char pctBuf[8];
    snprintf(pctBuf, sizeof(pctBuf), "%03d%%", (int)(fill * 100.0f));
    gfx.drawText(pctBuf, SCX, barY + 30, c, 1,
                 GfxEngine::TextAlign::CENTER, 191);

    // READY stamp — frame: 88×22, text at scale 2 is 16px tall
    if (stampT > 0.02f) {
        uint8_t sop = (uint8_t)(stampT * 255.0f);
        int16_t fy = SCREEN_H - 24;
        int16_t frameTop = fy - 12;
        gfx.strokeRoundedRect(SCX - 44, frameTop, 88, 22, 3, c, 2);
        gfx.drawText("READY", SCX, frameTop + 3, c, 2,
                     GfxEngine::TextAlign::CENTER, sop);
    }
}

// ─── Category registration ─────────────────────────────────────────
const VariantDef BOOT_VARIANTS[] = {
    {"boot-poweron",          "Power on",  2400, TONE_NONE, render_boot_poweron},
    {"boot-credits",          "Credits",   4500, TONE_NONE, render_boot_credits},
    {"boot-checks-personal",  "Self-test", 4200, TONE_NONE, render_boot_checks_personal},
    {"boot-ready-personal",   "Ready",     3000, TONE_NONE, render_boot_ready_personal},
};

extern const CategoryDef CAT_BOOT = {
    "boot", "Boot", ContentKind::STATUS, TONE_CYAN,
    BOOT_VARIANTS, sizeof(BOOT_VARIANTS) / sizeof(BOOT_VARIANTS[0]),
    false
};
