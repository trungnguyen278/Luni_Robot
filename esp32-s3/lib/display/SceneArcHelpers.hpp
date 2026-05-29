#pragma once
#include "GfxEngine.hpp"
#include "MathHelpers.hpp"
#include "ColorSystem.hpp"
#include <cstring>

// Phase splitter for scene-arc animations (port of JSX phases())
struct PhaseEntry {
    const char* name;
    float length;
};

struct PhaseResult {
    const char* name;
    float p;
    uint8_t index;
};

inline PhaseResult phases(float t, const PhaseEntry* list, uint8_t count) {
    float acc = 0.0f;
    for (uint8_t i = 0; i < count; i++) {
        if (t < acc + list[i].length || i == count - 1) {
            float p = list[i].length > 0.0f
                ? math::clamp((t - acc) / list[i].length, 0.0f, 1.0f)
                : 1.0f;
            return {list[i].name, p, i};
        }
        acc += list[i].length;
    }
    return {list[count - 1].name, 1.0f, (uint8_t)(count - 1)};
}

// Standard scene-arc transition result
struct SceneArcState {
    const char* phase;
    float p;
    float eyeScale;
    float eyeCx;
    float eyeCy;
    float sceneOp;
};

inline SceneArcState sceneArc(float t,
    float openLen = 0.10f, float transInLen = 0.12f,
    float actLen = 0.56f, float transOutLen = 0.12f,
    float cornerX = 278.0f, float cornerY = 44.0f,
    float cornerScale = 0.32f)
{
    using namespace math;
    using namespace geom;
    const float centerX = (float)SCX;
    const float centerY = (float)CY;
    SceneArcState s;

    if (t < openLen) {
        s.phase = "open"; s.p = openLen > 0 ? t / openLen : 1.0f;
        s.eyeScale = 1.0f; s.sceneOp = 0.0f;
        s.eyeCx = centerX; s.eyeCy = centerY;
    } else if (t < openLen + transInLen) {
        s.phase = "trans-in"; s.p = (t - openLen) / transInLen;
        float ep = ease::inOut(s.p);
        s.eyeScale = lerp(1.0f, cornerScale, ep);
        s.eyeCx = lerp(centerX, cornerX, ep);
        s.eyeCy = lerp(centerY, cornerY, ep);
        s.sceneOp = clamp((s.p - 0.05f) * 1.3f, 0.0f, 1.0f);
    } else if (t < openLen + transInLen + actLen) {
        s.phase = "act"; s.p = (t - openLen - transInLen) / actLen;
        s.eyeScale = cornerScale;
        s.eyeCx = cornerX; s.eyeCy = cornerY;
        s.sceneOp = 1.0f;
    } else if (t < openLen + transInLen + actLen + transOutLen) {
        s.phase = "trans-out";
        s.p = (t - openLen - transInLen - actLen) / transOutLen;
        float ep = ease::inOut(s.p);
        s.eyeScale = lerp(cornerScale, 1.0f, ep);
        s.eyeCx = lerp(cornerX, centerX, ep);
        s.eyeCy = lerp(cornerY, centerY, ep);
        s.sceneOp = clamp(1.0f - (s.p + 0.05f) * 1.3f, 0.0f, 1.0f);
    } else {
        float closeLen = 1.0f - openLen - transInLen - actLen - transOutLen;
        s.phase = "close";
        s.p = closeLen > 0 ? (t - openLen - transInLen - actLen - transOutLen) / closeLen : 1.0f;
        s.eyeScale = 1.0f; s.sceneOp = 0.0f;
        s.eyeCx = centerX; s.eyeCy = centerY;
    }
    return s;
}

// Eye emotion shapes for scenes (port of JSX LuniFace)
enum class EyeEmotion : uint8_t {
    STEADY, HAPPY, SATISFIED, SLEEPY, SURPRISED,
    EXCITED, EAGER, CURIOUS, SAD, THINKING, LOVE
};

inline EyeEmotion parseEyeEmotion(const char* name) {
    if (!name) return EyeEmotion::STEADY;
    if (strcmp(name, "happy") == 0)     return EyeEmotion::HAPPY;
    if (strcmp(name, "satisfied") == 0) return EyeEmotion::SATISFIED;
    if (strcmp(name, "sleepy") == 0)    return EyeEmotion::SLEEPY;
    if (strcmp(name, "surprised") == 0) return EyeEmotion::SURPRISED;
    if (strcmp(name, "excited") == 0)   return EyeEmotion::EXCITED;
    if (strcmp(name, "eager") == 0)     return EyeEmotion::EAGER;
    if (strcmp(name, "curious") == 0)   return EyeEmotion::CURIOUS;
    if (strcmp(name, "sad") == 0)       return EyeEmotion::SAD;
    if (strcmp(name, "thinking") == 0)  return EyeEmotion::THINKING;
    if (strcmp(name, "love") == 0)      return EyeEmotion::LOVE;
    return EyeEmotion::STEADY;
}

void drawLuniFace(GfxEngine& gfx, EyeEmotion emo, float t, uint16_t eyeColor, uint16_t bgColor);

// Draw Luni face at (cx,cy) with scale — the TransformedFace equivalent
inline void drawPlacedEyes(GfxEngine& gfx, float cx, float cy, float scale,
                           const char* emotion, float t,
                           uint16_t eyeColor, uint16_t bgColor = COLOR_BG) {
    gfx.pushTransform();
    gfx.translate(cx, cy);
    gfx.scale(scale, scale);
    gfx.translate(-(float)geom::SCX, -(float)geom::CY);
    drawLuniFace(gfx, parseEyeEmotion(emotion), t, eyeColor, bgColor);
    gfx.popTransform();
}

// Bottom-of-screen status label
inline void drawActLabel(GfxEngine& gfx, const char* text,
                         uint16_t color, uint8_t alpha = 217) {
    gfx.drawText(text, geom::SCX, geom::SCREEN_H - 12, color, 1,
                 GfxEngine::TextAlign::CENTER, alpha);
}

// Phase name matcher helper
inline bool phaseIs(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

inline bool phaseIn(const char* name, const char* a, const char* b) {
    return strcmp(name, a) == 0 || strcmp(name, b) == 0;
}

inline bool phaseIn(const char* name, const char* a, const char* b, const char* c) {
    return strcmp(name, a) == 0 || strcmp(name, b) == 0 || strcmp(name, c) == 0;
}

inline bool phaseIn(const char* name, const char* a, const char* b,
                    const char* c, const char* d) {
    return strcmp(name, a) == 0 || strcmp(name, b) == 0 ||
           strcmp(name, c) == 0 || strcmp(name, d) == 0;
}
