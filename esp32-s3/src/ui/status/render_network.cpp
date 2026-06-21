#include "ui/VariantRegistry.hpp"
#include "ui/SceneManager.hpp"
#include "system/StateManager.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace geom;
using namespace math;

// WiFi icon: 3 concentric arcs (top-half) + dot at bottom
static void drawWifiIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                         int bars, uint16_t color, uint8_t alpha = 255) {
    int16_t radii[] = {28, 18, 8};
    for (int i = 0; i < 3; i++) {
        uint8_t op = (i < (3 - bars)) ? (uint8_t)(alpha * 0.2f) : alpha;
        // Top-half arc: from -PI to 0 (left to right over top)
        gfx.drawArc(cx, cy, radii[i], -3.14159f, 0.0f, color, 5, op);
    }
    gfx.fillCircle(cx, cy + 3, 3, color, alpha);
}

// BT icon simplified as lines
static void drawBTIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                       uint16_t color, uint8_t alpha = 255) {
    // Vertical center line
    gfx.drawLine(cx, cy - 16, cx, cy + 16, color, 4);
    // Upper right arrow
    gfx.drawLine(cx, cy - 16, cx + 10, cy - 8, color, 4);
    gfx.drawLine(cx + 10, cy - 8, cx - 10, cy + 8, color, 4);
    // Lower right arrow
    gfx.drawLine(cx, cy + 16, cx + 10, cy + 8, color, 4);
    gfx.drawLine(cx + 10, cy + 8, cx - 10, cy - 8, color, 4);
}

// Typing dots animation
static void drawTypingDots(GfxEngine& gfx, int16_t cx, int16_t cy, float t,
                           uint16_t color) {
    int step = ((int)(t * 6.0f)) % 3;
    for (int i = -1; i <= 1; i++) {
        uint8_t op = (i == step - 1) ? 255 : 77;
        gfx.fillCircle((int16_t)(cx + i * 12), cy, 4, color, op);
    }
}

// network-wifi-scan: cycling WiFi bars + "SEARCHING" text
static void render_wifi_scan(GfxEngine& gfx, float t, const ColorContext& colors) {
    float phase = fmodf(t * 3.0f, 1.0f);
    int bars = phase < 0.33f ? 1 : phase < 0.66f ? 2 : 3;
    drawWifiIcon(gfx, SCX, SCY - 14, bars, colors.accent);

    gfx.drawText("SEARCHING WI-FI", SCX, SCREEN_H - 40, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 242);
    drawTypingDots(gfx, SCX, SCREEN_H - 20, t, colors.accent);
}

// network-wifi-connect: full bars blinking + progress bar + SSID
static void render_wifi_connect(GfxEngine& gfx, float t, const ColorContext& colors) {
    uint8_t blink = ((int)(t * 4.0f) % 2 == 0) ? 255 : 102;
    drawWifiIcon(gfx, SCX, STATUS_H + 56, 3, colors.accent, blink);

    gfx.drawText("CONNECTING", SCX, STATUS_H + 104, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);

    const auto& sd = SceneManager::instance().getSceneData();
    const char* ssid = (sd.ssid[0] != '\0') ? sd.ssid : "WI-FI";
    gfx.drawText(ssid, SCX, STATUS_H + 120, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 217);

    // Progress bar
    int16_t barW = SCREEN_W - 80;
    int16_t barX = (SCREEN_W - barW) / 2;
    int16_t barY = SCREEN_H - 38;
    gfx.strokeRoundedRect(barX, barY, barW, 8, 2, colors.eye, 1);
    int16_t fw = (int16_t)((float)(barW - 3) * clamp(t, 0.0f, 1.0f));
    if (fw > 0) {
        gfx.fillRoundedRect(barX + 1, barY + 1, fw, 6, 1, colors.accent);
    }
}

// network-wifi-retry: spinning retry ring + attempt counter
static void render_wifi_retry(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawWifiIcon(gfx, SCX, STATUS_H + 60, 1, colors.accent, 217);

    // Spinning retry arc
    float spin = t * TAU;
    gfx.drawArc(SCX, STATUS_H + 60, 42, spin, spin + 4.7f,
                colors.accent, 3);

    gfx.drawText("RETRY CONNECTION", SCX, STATUS_H + 126, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);

    int attempt = ((int)(t * 3.0f)) + 1;
    char buf[20];
    snprintf(buf, sizeof(buf), "ATTEMPT %d OF 3", attempt);
    gfx.drawText(buf, SCX, SCREEN_H - 18, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER);
}

// network-offline: crossed-out WiFi + "NO INTERNET"
static void render_offline(GfxEngine& gfx, float t, const ColorContext& colors) {
    drawWifiIcon(gfx, SCX, STATUS_H + 60, 0, colors.eye, 115);

    // Diagonal slash
    gfx.drawLine(SCX - 36, STATUS_H + 30, SCX + 36, STATUS_H + 100,
                 colors.accent, 5);

    float breathe = 0.7f + fabsf(sinf(t * TAU)) * 0.3f;
    gfx.drawText("NO INTERNET", SCX, STATUS_H + 130, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, (uint8_t)(breathe * 255.0f));
    gfx.drawText("CHECK WI-FI ROUTER", SCX, SCREEN_H - 18, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// network-bt-scan: BT icon with expanding pulse rings
static void render_ble_pair(GfxEngine& gfx, float t, const ColorContext& colors) {
    // Pulse rings
    for (int w = 0; w < 2; w++) {
        float p = fmodf(t + w * 0.5f, 1.0f);
        int16_t r = (int16_t)lerp(18.0f, 56.0f, p);
        uint8_t op = (uint8_t)((1.0f - p) * 0.7f * 255.0f);
        gfx.pushAlpha(op);
        gfx.strokeCircle(SCX, STATUS_H + 60, r, colors.accent, 2);
        gfx.popAlpha();
    }

    drawBTIcon(gfx, SCX, STATUS_H + 60, colors.accent);

    gfx.drawText("SEARCHING", SCX, STATUS_H + 126, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);
    drawTypingDots(gfx, SCX, STATUS_H + 146, t, colors.accent);

    gfx.drawText("OPEN APP", SCX, SCREEN_H - 26, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 217);
    gfx.drawText("TO CONNECT", SCX, SCREEN_H - 12, colors.accent, 1,
                 GfxEngine::TextAlign::CENTER, 140);
}

// network-server-error: cloud icon + state-aware text
static void render_server_error(GfxEngine& gfx, float t, const ColorContext& colors) {
    auto connState = StateManager::instance().getConnectionState();

    bool isConnecting = (connState == state::ConnectionState::WS_CONNECTING ||
                         connState == state::ConnectionState::WS_AUTHENTICATING);

    float shake = isConnecting ? 0.0f : sinf(t * TAU * 6.0f) * 1.5f;
    uint8_t blink = isConnecting
        ? (uint8_t)(180 + sinf(t * TAU * 2.0f) * 75.0f)
        : ((int)(t * 3.0f) % 2 == 0) ? 255 : 128;

    gfx.pushTransform();
    gfx.translate(shake, 0.0f);

    // Cloud icon
    gfx.fillEllipse(SCX - 10, STATUS_H + 36, 16, 12, colors.accent, blink);
    gfx.fillEllipse(SCX + 10, STATUS_H + 36, 18, 14, colors.accent, blink);
    gfx.fillEllipse(SCX, STATUS_H + 28, 14, 10, colors.accent, blink);
    gfx.fillRoundedRect(SCX - 24, STATUS_H + 40, 50, 10, 5, colors.accent, blink);

    if (isConnecting) {
        // Spinning arc below cloud (connecting indicator)
        float spin = t * TAU * 2.0f;
        gfx.drawArc(SCX, STATUS_H + 68, 14, spin, spin + 4.0f, colors.accent, 3);

        gfx.drawText("CONNECTING SERVER", SCX, STATUS_H + 114, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER);
        drawTypingDots(gfx, SCX, STATUS_H + 134, t, colors.accent);
        gfx.drawText("PLEASE WAIT", SCX, SCREEN_H - 18, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 140);
    } else {
        // Exclamation mark below cloud (error indicator)
        gfx.fillRoundedRect(SCX - 3, STATUS_H + 56, 6, 20, 2, colors.accent, blink);
        gfx.fillCircle(SCX, STATUS_H + 82, 3, colors.accent, blink);

        const char* title = "SERVER UNREACHABLE";
        const char* code  = "CONNECTION FAILED";
        const char* hint  = "RETRYING IN A MOMENT";

        if (connState == state::ConnectionState::WS_ERROR) {
            code = "WS ERROR";
        } else if (connState == state::ConnectionState::RECONNECTING) {
            title = "RECONNECTING";
            code = "LOST CONNECTION";
        }

        gfx.drawText(title, SCX, STATUS_H + 114, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER);
        gfx.drawText(code, SCX, STATUS_H + 132, colors.accent, 1,
                     GfxEngine::TextAlign::CENTER, 217);
        gfx.drawText(hint, SCX, SCREEN_H - 18, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 140);
    }

    gfx.popTransform();
}

// network-bt-paired: BT icon + phone + check mark + "BLUETOOTH ON"
static void render_bt_paired(GfxEngine& gfx, float t, const ColorContext& colors) {
    float pop = t < 0.25f ? ease::out(t / 0.25f) : 1.0f;
    float wave = sinf(t * TAU) * 0.5f + 0.5f;
    uint8_t btOp = (uint8_t)(217 + wave * 38);

    // BT icon on left
    drawBTIcon(gfx, SCX - 60, SCY - 10, colors.accent, btOp);

    // Phone shape on right
    gfx.fillRoundedRect(SCX + 44, SCY - 36, 32, 52, 4, colors.accent);
    gfx.fillRoundedRect(SCX + 48, SCY - 32, 24, 40, 2, colors.bg, 102);
    gfx.fillCircle(SCX + 60, SCY + 10, 2, colors.bg, 255);

    // Dashed link line
    gfx.drawDashedLine(SCX - 38, SCY - 10, SCX + 40, SCY - 10,
                       colors.accent, 2, 4, 4);

    // Center check circle (pops in)
    int16_t checkR = (int16_t)(14.0f * pop);
    gfx.fillCircle(SCX, SCY - 10, checkR, colors.accent);
    if (pop > 0.5f) {
        // Checkmark inside circle
        gfx.drawLine(SCX - 6, SCY - 10, SCX - 1, SCY - 5, colors.bg, 3);
        gfx.drawLine(SCX - 1, SCY - 5, SCX + 7, SCY - 15, colors.bg, 3);
    }

    gfx.drawText("CONNECTED", SCX, SCY + 46, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);

    const auto& sd = SceneManager::instance().getSceneData();
    if (sd.ble_pin[0] != '\0') {
        gfx.drawText("PIN", SCX, SCY + 66, colors.eye, 1,
                     GfxEngine::TextAlign::CENTER, 140);
        gfx.drawText(sd.ble_pin, SCX, SCY + 82, colors.accent, 2,
                     GfxEngine::TextAlign::CENTER);
    }
}

// network-ws-connect: WiFi OK, connecting to server
static void render_ws_connect(GfxEngine& gfx, float t, const ColorContext& colors) {
    // WiFi icon solid (connected)
    drawWifiIcon(gfx, SCX - 80, STATUS_H + 48, 3, colors.eye, 140);

    // Server icon (stacked rects)
    int16_t sx = SCX + 60, sy = STATUS_H + 36;
    for (int i = 0; i < 3; i++) {
        int16_t ry = (int16_t)(sy + i * 18);
        gfx.fillRoundedRect(sx - 20, ry, 40, 14, 3, colors.accent);
        gfx.fillCircle(sx - 12, ry + 7, 2, colors.bg);
        gfx.fillCircle(sx + 12, ry + 7, 2, colors.bg);
    }

    // Connecting arrow between wifi and server
    float pulse = (sinf(t * TAU * 2.0f) + 1.0f) / 2.0f;
    uint8_t arrowOp = (uint8_t)(128 + pulse * 127);
    int16_t ax = SCX - 20, ay = STATUS_H + 50;
    gfx.drawLine(ax, ay, (int16_t)(ax + 28), ay, colors.accent, 3, arrowOp);
    gfx.drawLine((int16_t)(ax + 22), (int16_t)(ay - 5),
                 (int16_t)(ax + 28), ay, colors.accent, 3, arrowOp);
    gfx.drawLine((int16_t)(ax + 22), (int16_t)(ay + 5),
                 (int16_t)(ax + 28), ay, colors.accent, 3, arrowOp);

    gfx.drawText("CONNECTING SERVER", SCX, STATUS_H + 110, colors.eye, 1,
                 GfxEngine::TextAlign::CENTER);
    drawTypingDots(gfx, SCX, STATUS_H + 130, t, colors.accent);

    // Progress bar
    int16_t barW = SCREEN_W - 80;
    int16_t barX = (SCREEN_W - barW) / 2;
    int16_t barY = SCREEN_H - 38;
    gfx.strokeRoundedRect(barX, barY, barW, 8, 2, colors.eye, 1);
    int16_t fw = (int16_t)((float)(barW - 3) * clamp(t, 0.0f, 1.0f));
    if (fw > 0) {
        gfx.fillRoundedRect(barX + 1, barY + 1, fw, 6, 1, colors.accent);
    }
}

// --- Category registration ---
const VariantDef NETWORK_VARIANTS[] = {
    {"network-wifi-scan",     "Searching",   1800, TONE_CYAN,   render_wifi_scan},
    {"network-wifi-connect",  "Connecting",  2400, TONE_CYAN,   render_wifi_connect},
    {"network-wifi-retry",    "Retrying",    2000, TONE_ORANGE, render_wifi_retry},
    {"network-offline",       "Offline",     2400, TONE_RED,    render_offline},
    {"network-bt-scan",      "BLE pairing", 2200, TONE_PURPLE, render_ble_pair},
    {"network-bt-paired",     "Bluetooth on",2600, TONE_PURPLE, render_bt_paired},
    {"network-ws-connect",    "Connecting srv",2400, TONE_CYAN, render_ws_connect},
    {"network-server-error",  "Server down", 2400, TONE_RED,    render_server_error},
};

extern const CategoryDef CAT_NETWORK = {
    "network", "Network", ContentKind::STATUS, TONE_CYAN,
    NETWORK_VARIANTS, sizeof(NETWORK_VARIANTS) / sizeof(NETWORK_VARIANTS[0])
};
