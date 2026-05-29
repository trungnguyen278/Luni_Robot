#include "SceneArcHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

static void drawRectEyes(GfxEngine& gfx, float h, float w, float yOff,
                         float rx, uint16_t color) {
    int16_t iw = (int16_t)w, ih = (int16_t)h, irx = (int16_t)rx;
    int16_t ly = (int16_t)(CY + yOff - h / 2.0f);
    int16_t ry = ly;
    gfx.fillRoundedRect(LX - iw / 2, ly, iw, ih, irx, color);
    gfx.fillRoundedRect(RX - iw / 2, ry, iw, ih, irx, color);
}

static void drawArcSmile(GfxEngine& gfx, float depth, float yOff,
                         int16_t sw, uint16_t color) {
    int16_t hw = (EYE_W - 10) / 2;
    int16_t cy = (int16_t)(CY + yOff);
    gfx.drawQuadBezier(LX - hw, cy, LX, (int16_t)(cy + depth), LX + hw, cy,
                       color, sw);
    gfx.drawQuadBezier(RX - hw, cy, RX, (int16_t)(cy + depth), RX + hw, cy,
                       color, sw);
}

void drawLuniFace(GfxEngine& gfx, EyeEmotion emo, float t,
                  uint16_t eyeColor, uint16_t bgColor) {
    switch (emo) {
    case EyeEmotion::HAPPY:
        drawArcSmile(gfx, 26.0f, 10.0f, 14, eyeColor);
        break;

    case EyeEmotion::SATISFIED:
        drawArcSmile(gfx, 16.0f, 8.0f, 12, eyeColor);
        break;

    case EyeEmotion::SLEEPY: {
        float droop = sinf(t * TAU * 0.6f) * 2.0f;
        drawRectEyes(gfx, EYE_H * 0.32f, (float)EYE_W, 22.0f + droop, 12.0f, eyeColor);
        break;
    }

    case EyeEmotion::SURPRISED:
        drawRectEyes(gfx, EYE_H * 1.05f, EYE_W * 1.08f, 0.0f, 36.0f, eyeColor);
        break;

    case EyeEmotion::EXCITED:
    case EyeEmotion::EAGER: {
        float wob = sinf(t * TAU * 3.0f) * 2.0f;
        drawRectEyes(gfx, EYE_H * 1.05f, EYE_W * 1.05f, -2.0f + wob, 30.0f, eyeColor);
        break;
    }

    case EyeEmotion::CURIOUS: {
        float tilt = sinf(t * TAU * 0.7f) * 4.0f;
        gfx.pushTransform();
        gfx.rotate(tilt, (float)SCX, (float)CY);
        drawRectEyes(gfx, EYE_H * 0.95f, (float)EYE_W, -2.0f, 22.0f, eyeColor);
        gfx.popTransform();
        break;
    }

    case EyeEmotion::SAD: {
        float h = EYE_H * 0.7f;
        float y = (float)CY + 6.0f;
        int16_t iy = (int16_t)(y - h / 2.0f);
        int16_t ih = (int16_t)h;
        int16_t hw = EYE_W / 2;
        gfx.fillRoundedRect(LX - hw, iy, EYE_W, ih, 18, eyeColor);
        gfx.fillRoundedRect(RX - hw, iy, EYE_W, ih, 18, eyeColor);
        // Slanted bg lids
        int16_t topY = (int16_t)(y - (float)EYE_H / 2.0f - 4);
        int16_t midY = (int16_t)(y - 4.0f);
        int16_t lidLow = (int16_t)(y + 20.0f);
        gfx.fillTriangle(LX - hw - 4, topY, LX + hw + 4, topY,
                         LX + hw + 4, midY, bgColor);
        gfx.fillTriangle(LX - hw - 4, topY, LX + hw + 4, midY,
                         LX - hw - 4, lidLow, bgColor);
        gfx.fillTriangle(RX - hw - 4, topY, RX + hw + 4, topY,
                         RX + hw + 4, lidLow, bgColor);
        gfx.fillTriangle(RX - hw - 4, topY, RX + hw + 4, lidLow,
                         RX - hw - 4, midY, bgColor);
        break;
    }

    case EyeEmotion::THINKING: {
        float look = sinf(t * TAU * 0.8f) * 6.0f;
        drawRectEyes(gfx, EYE_H * 0.9f, (float)EYE_W, 0.0f, 22.0f, eyeColor);
        gfx.fillCircle((int16_t)(LX + look), CY - 8, 12, bgColor);
        gfx.fillCircle((int16_t)(RX + look), CY - 8, 12, bgColor);
        break;
    }

    case EyeEmotion::LOVE: {
        float sz = (float)EYE_W * 0.9f;
        float k = sz / 30.0f;
        auto drawHeart = [&](int16_t cx) {
            int16_t r = (int16_t)(9.0f * k);
            int16_t bumpY = (int16_t)((float)CY - 9.0f * k);
            int16_t tipY  = (int16_t)((float)CY + 9.0f * k);
            gfx.fillCircle((int16_t)(cx - r), bumpY, r, eyeColor);
            gfx.fillCircle((int16_t)(cx + r), bumpY, r, eyeColor);
            gfx.fillTriangle((int16_t)(cx - 2 * r), bumpY,
                             (int16_t)(cx + 2 * r), bumpY,
                             cx, tipY, eyeColor);
        };
        drawHeart(LX);
        drawHeart(RX);
        break;
    }

    case EyeEmotion::STEADY:
    default: {
        float microBlink = fmaxf(blink(t, 0.4f, 0.05f), blink(t, 0.9f, 0.05f));
        float h = (float)EYE_H * (1.0f - microBlink * 0.92f);
        drawRectEyes(gfx, h, (float)EYE_W, 0.0f, 22.0f, eyeColor);
        break;
    }
    }
}
