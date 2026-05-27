# Rendering Architecture — GfxEngine

Design document for the procedural RGB565 rendering engine that replaces
the 1-bit RLE animation player for emotions and scenes.

---

## 1. Why Procedural Rendering

### Flash budget analysis

| Board | Flash | SPIFFS | App code |
|-------|-------|--------|----------|
| N4R8 (current) | 4 MB | 960 KB | 3 MB |
| N16R8 (target) | 16 MB | 12.9 MB | 3 MB |

Pre-rendered approach estimate for 227 variants:
```
227 variants x avg 60 frames x 320x240x2 bytes = ~1.05 GB uncompressed
Even at 10:1 RLE compression = ~105 MB
Even at 50:1 aggressive compression = ~21 MB → exceeds SPIFFS
```

Procedural approach:
```
227 render functions x avg 350 bytes compiled = ~80 KB
GfxEngine library = ~30 KB
Total = ~110 KB (< 0.01% of flash)
```

### PSRAM enables framebuffer

The ESP32-S3 N16R8 has 8 MB PSRAM (octal SPI, 80 MHz).
Full framebuffer: `320 x 240 x 2 = 153,600 bytes` (~150 KB).
Double buffer: 300 KB. Both fit easily in 8 MB.

### Design system is vector-native

The JSX design uses only simple geometric primitives (rounded rects,
circles, arcs, lines, paths, text). These translate directly to 2D
draw calls — no rasterization of complex artwork needed.

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────┐
│  SceneManager / EmotionLoop                 │
│  (picks variant, manages t, injects data)   │
│         │                                   │
│         ▼                                   │
│  render_variant(gfx, t, colors)             │  ← per-variant C++ function
│         │                                   │
│         ▼                                   │
│  GfxEngine                                  │
│  ┌───────────────────────────────────┐      │
│  │ PSRAM Framebuffer (320x240 RGB565)│      │
│  │                                   │      │
│  │ Shape Primitives:                 │      │
│  │  fillRoundedRect, fillCircle,     │      │
│  │  drawLine, drawArc, drawBezier,   │      │
│  │  fillPolygon, drawText            │      │
│  │                                   │      │
│  │ Transform Stack:                  │      │
│  │  push/pop, translate, rotate,     │      │
│  │  scale                            │      │
│  │                                   │      │
│  │ Alpha Blending                    │      │
│  └───────────────────────────────────┘      │
│         │                                   │
│         ▼                                   │
│  DisplayDriver (ST7789 SPI)                 │
│  flush() → setWindow → writePixels (DMA)    │
└─────────────────────────────────────────────┘
```

### Render loop (every frame)

```cpp
void DisplayManager::renderFrame(float dt_ms) {
    // 1. Advance time
    elapsed_ms_ += dt_ms;
    float t = fmodf(elapsed_ms_ / current_variant_->duration_ms, 1.0f);

    // 2. Clear framebuffer
    gfx_->clear(GfxEngine::COLOR_BG);

    // 3. Render variant
    ColorContext colors = resolveColors(current_category_, current_variant_);
    current_variant_->render(*gfx_, t, colors);

    // 4. Render status bar (always on top)
    renderStatusBar(*gfx_, colors);

    // 5. Flush to display
    gfx_->flush(drv_);
}
```

---

## 3. GfxEngine API

### 3.1 Class Declaration

```cpp
// esp32-s3/lib/display/GfxEngine.hpp
#pragma once
#include <cstdint>

class DisplayDriver;

class GfxEngine {
public:
    GfxEngine(uint16_t width, uint16_t height);
    ~GfxEngine();

    bool init();

    // --- Framebuffer ---
    uint16_t* getFramebuffer();
    void clear(uint16_t color = COLOR_BG);

    // --- Filled Shapes ---
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                  uint16_t color, uint8_t alpha = 255);

    void fillRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                         int16_t rx, uint16_t color, uint8_t alpha = 255);

    void fillCircle(int16_t cx, int16_t cy, int16_t r,
                    uint16_t color, uint8_t alpha = 255);

    void fillEllipse(int16_t cx, int16_t cy, int16_t rx, int16_t ry,
                     uint16_t color, uint8_t alpha = 255);

    void fillPolygon(const int16_t* xPts, const int16_t* yPts,
                     int16_t count, uint16_t color, uint8_t alpha = 255);

    // --- Stroked Shapes ---
    void strokeCircle(int16_t cx, int16_t cy, int16_t r,
                      uint16_t color, int16_t width = 1);

    void strokeRoundedRect(int16_t x, int16_t y, int16_t w, int16_t h,
                           int16_t rx, uint16_t color, int16_t strokeWidth = 1);

    void drawLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                  uint16_t color, int16_t width = 1, uint8_t alpha = 255);

    void drawDashedLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        uint16_t color, int16_t width,
                        int16_t dashLen, int16_t gapLen);

    void drawArc(int16_t cx, int16_t cy, int16_t r,
                 float startAngle, float endAngle,
                 uint16_t color, int16_t width = 1, uint8_t alpha = 255);

    void drawQuadBezier(int16_t x0, int16_t y0,
                        int16_t cx, int16_t cy,
                        int16_t x1, int16_t y1,
                        uint16_t color, int16_t width = 1);

    void drawCubicBezier(int16_t x0, int16_t y0,
                         int16_t cx1, int16_t cy1,
                         int16_t cx2, int16_t cy2,
                         int16_t x1, int16_t y1,
                         uint16_t color, int16_t width = 1);

    // --- Text ---
    enum class TextAlign : uint8_t { LEFT, CENTER, RIGHT };

    void drawText(const char* text, int16_t x, int16_t y,
                  uint8_t fontSize, uint16_t color,
                  TextAlign align = TextAlign::LEFT, uint8_t alpha = 255);

    // --- High-level Helpers (matching JSX primitives) ---
    void drawEye(int16_t cx, int16_t cy, int16_t w, int16_t h,
                 int16_t rx, float rotation, uint16_t color, uint8_t alpha = 255);

    void drawEyes(int16_t lx, int16_t ly, int16_t lw, int16_t lh,
                  int16_t rx_pos, int16_t ry, int16_t rw, int16_t rh,
                  int16_t cornerR, uint16_t color);

    void fillHeart(int16_t cx, int16_t cy, int16_t size, uint16_t color);
    void fillStar(int16_t cx, int16_t cy, int16_t r1, int16_t r2, uint16_t color);

    // --- Transform Stack ---
    void pushTransform();
    void popTransform();
    void translate(float dx, float dy);
    void rotate(float angleDeg, float cx, float cy);
    void scale(float sx, float sy);

    // --- Alpha Stack ---
    void pushAlpha(uint8_t alpha);
    void popAlpha();

    // --- Flush ---
    void flush(DisplayDriver* drv);

    // --- Constants ---
    static constexpr uint16_t COLOR_BG = 0x0041;

private:
    uint16_t* framebuffer_ = nullptr;
    uint16_t width_, height_;

    // Transform stack (max depth 8)
    struct AffineTransform {
        float a = 1, b = 0, c = 0, d = 1, tx = 0, ty = 0;
    };
    static constexpr int MAX_TRANSFORM_DEPTH = 8;
    AffineTransform transform_stack_[MAX_TRANSFORM_DEPTH];
    int transform_depth_ = 0;
    AffineTransform current_transform_;

    // Alpha stack (max depth 8)
    static constexpr int MAX_ALPHA_DEPTH = 8;
    uint8_t alpha_stack_[MAX_ALPHA_DEPTH];
    int alpha_depth_ = 0;
    uint8_t current_alpha_ = 255;

    // Internal
    void setPixel(int16_t x, int16_t y, uint16_t color, uint8_t alpha);
    void hLine(int16_t x, int16_t y, int16_t w, uint16_t color, uint8_t alpha);
    uint16_t blendRGB565(uint16_t bg, uint16_t fg, uint8_t alpha);
    void applyTransform(float& x, float& y);
};
```

### 3.2 Implementation Notes

**Framebuffer allocation** (in `init()`):
```cpp
bool GfxEngine::init() {
    framebuffer_ = (uint16_t*)heap_caps_malloc(
        width_ * height_ * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    return framebuffer_ != nullptr;
}
```

**Alpha blending** (fast 5-6-5 blend):
```cpp
uint16_t GfxEngine::blendRGB565(uint16_t bg, uint16_t fg, uint8_t alpha) {
    if (alpha == 255) return fg;
    if (alpha == 0) return bg;
    uint8_t inv = 255 - alpha;
    uint16_t rb_bg = bg & 0xF81F;
    uint16_t g_bg  = bg & 0x07E0;
    uint16_t rb_fg = fg & 0xF81F;
    uint16_t g_fg  = fg & 0x07E0;
    uint16_t rb = ((rb_fg * alpha + rb_bg * inv) >> 8) & 0xF81F;
    uint16_t g  = ((g_fg  * alpha + g_bg  * inv) >> 8) & 0x07E0;
    return rb | g;
}
```

**fillRoundedRect** (critical for Eye rendering):
Uses scanline fill with circle quadrant offsets for corners. For each
scanline y in `[0, h)`:
- If within the rounded corner region, compute x-offset from circle equation
- Draw horizontal span for that scanline

**Flush to display**:
```cpp
void GfxEngine::flush(DisplayDriver* drv) {
    drv->setWindow(0, 0, width_ - 1, height_ - 1);
    drv->writePixels(framebuffer_, width_ * height_ * 2);
}
```

At 40 MHz SPI, flushing 153,600 bytes takes ~30 ms. Use DMA to overlap
with next frame's rendering when possible.

---

## 4. ColorContext System

```cpp
// esp32-s3/lib/display/ColorSystem.hpp
#pragma once
#include <cstdint>

enum ToneId : uint8_t {
    TONE_CYAN = 0,
    TONE_WARM,
    TONE_ROSE,
    TONE_RED,
    TONE_BLUE,
    TONE_GREEN,
    TONE_PURPLE,
    TONE_ORANGE,
    TONE_WHITE,
    TONE_COUNT,
    TONE_NONE = 0xFF
};

// RGB565 lookup table (indexed by ToneId)
constexpr uint16_t TONE_LUT[TONE_COUNT] = {
    0x5F9F,  // cyan   #5be9ff
    0xFE8C,  // warm   #ffd166
    0xFB53,  // rose   #ff6b9d
    0xFACE,  // red    #ff5b6e
    0x75DF,  // blue   #76b8ff
    0x7F51,  // green  #7be88e
    0xB47F,  // purple #b48cff
    0xFCEB,  // orange #ff9d5b
    0xF7BF,  // white  #f0f4ff
};

constexpr uint16_t COLOR_BG = 0x0041;  // #06090d

struct ColorContext {
    uint16_t eye;            // var(--eye)
    uint16_t accent;         // var(--accent)
    uint16_t bg;             // var(--bg)
    uint16_t tones[TONE_COUNT]; // all tones for multi-color

    static ColorContext forEmotion(ToneId tone) {
        ColorContext ctx;
        ctx.eye    = TONE_LUT[TONE_CYAN];
        ctx.accent = TONE_LUT[tone < TONE_COUNT ? tone : TONE_CYAN];
        ctx.bg     = COLOR_BG;
        for (int i = 0; i < TONE_COUNT; i++) ctx.tones[i] = TONE_LUT[i];
        return ctx;
    }

    static ColorContext forScene(ToneId tone) {
        ToneId t = tone < TONE_COUNT ? tone : TONE_CYAN;
        ColorContext ctx;
        ctx.eye    = TONE_LUT[t];
        ctx.accent = TONE_LUT[t];
        ctx.bg     = COLOR_BG;
        for (int i = 0; i < TONE_COUNT; i++) ctx.tones[i] = TONE_LUT[i];
        return ctx;
    }
};
```

---

## 5. Backward Compatibility

During migration, the system supports both rendering paths:

```
DisplayManager
  ├── GfxEngine (new) → procedural render functions
  └── AnimationPlayer (legacy) → 1-bit RLE assets

On emotion change:
  1. Check VariantRegistry for procedural render function
  2. If found → use GfxEngine path
  3. If not found → fall back to AnimationPlayer (legacy 1-bit RLE)
```

The 7 existing RLE emotions (happy, idle, listening, neutral, sad, stun,
thinking) continue working via AnimationPlayer until their procedural
equivalents are ported.

---

## 6. Performance Budget

Target: **30 fps** = 33.3 ms per frame.

| Phase | Budget | Notes |
|-------|--------|-------|
| Clear framebuffer | ~0.5 ms | memset 150KB in PSRAM |
| Render variant | 5-15 ms | Depends on complexity |
| Render status bar | ~1 ms | Simple text + icons |
| Flush via SPI DMA | ~30 ms | 153KB at 40 MHz |
| **Total** | **36-46 ms** | ~22-27 fps without DMA overlap |

### Optimizations

1. **DMA overlap**: Start DMA flush, then render next frame in parallel.
   Requires double buffer (300 KB PSRAM — still fits).

2. **Dirty region tracking**: Track bounding box of changed pixels. Only
   flush the dirty region instead of full screen. Effective for small
   animations (e.g., blinking eyes only changes ~100 scanlines).

3. **Fixed-point math**: Use 16.16 fixed-point for sin/cos in tight loops
   (8 rays × per frame). Pre-compute 256-entry sine LUT.

4. **Scanline-oriented rendering**: Process framebuffer top-to-bottom for
   cache-friendly PSRAM access.

5. **Reduce SPI clock impact**: Consider SPI at 80 MHz (ST7789 supports up
   to 80 MHz in some configurations) to halve flush time to ~15 ms.

### With DMA double-buffering at 30 fps

```
Frame N:   [render ~12ms] [idle ~21ms]
SPI DMA:   [flush frame N-1 ~30ms    ]
                          ↕ overlap
Frame N+1: [render starts when flush completes]
```

Effective FPS: limited by max(render_time, flush_time) = ~30 ms = 33 fps.
Achievable at 30 fps with DMA overlap.

---

## 7. Font System

### Current: Font8x8

The existing `Font8x8.hpp` provides an 8x8 pixel monospace bitmap font.
This is sufficient for status bar time display but too small/crude for
scene text like "SYSTEM CHECK", "28°", "READY".

### Recommended: Multi-size bitmap font

Store 3 font sizes matching the JSX design's usage:

| Size ID | Pixel Height | JSX fontSize equiv | Usage |
|---------|-------------|-------------------|-------|
| FONT_9 | 9 px | `fontSize={9}` | Footer text, small labels |
| FONT_11 | 11 px | `fontSize={11}` | Status bar, scene labels, check items |
| FONT_14 | 14 px | `fontSize={14}` | "READY" stamp, emphasis |
| FONT_22 | 22 px | `fontSize={22}` | "Luni" text in boot logo |
| FONT_36 | 36 px | `fontSize={36}` | Temperature "28°" |

Font data stored as bitmap arrays in flash. The monospace pixel aesthetic
matches the "robot display" design language.

Storage estimate: ~5 glyph sizes × 95 printable ASCII × avg 20 bytes = ~9.5 KB.

---

## 8. DisplayManager Refactoring

### New members

```cpp
class DisplayManager {
    // ... existing members ...

    // New rendering engine
    GfxEngine* gfx_ = nullptr;

    // Scene/emotion management
    SceneManager* scene_mgr_ = nullptr;
    VariantRegistry* registry_ = nullptr;

    // Render mode
    enum class RenderMode { LEGACY_RLE, PROCEDURAL };
    RenderMode render_mode_ = RenderMode::LEGACY_RLE;

    // Frame timing
    float elapsed_ms_ = 0;
    const VariantDef* current_variant_ = nullptr;
    const CategoryDef* current_category_ = nullptr;
};
```

### Initialization

```cpp
bool DisplayManager::init() {
    // Existing display driver init
    drv_ = new DisplayDriver();
    drv_->init(config);

    // New: GfxEngine
    gfx_ = new GfxEngine(320, 240);
    if (!gfx_->init()) {
        // PSRAM allocation failed — fall back to legacy mode
        render_mode_ = RenderMode::LEGACY_RLE;
    } else {
        render_mode_ = RenderMode::PROCEDURAL;
    }

    // Scene manager
    scene_mgr_ = new SceneManager();
    registry_ = new VariantRegistry();

    return true;
}
```

### Update loop

```cpp
void DisplayManager::update(uint32_t dt_ms) {
    if (render_mode_ == RenderMode::PROCEDURAL && current_variant_) {
        renderFrame((float)dt_ms);
    } else if (render_mode_ == RenderMode::LEGACY_RLE) {
        anim_player_->update(dt_ms);
        anim_player_->render();
    }
}
```

---

## 9. Implementation Order

| Step | Component | Deliverable |
|------|-----------|-------------|
| 1 | `MathHelpers.hpp` | lerp, clamp, ease, pulse, blink, sin LUT |
| 2 | `ColorSystem.hpp` | ToneId enum, TONE_LUT, ColorContext |
| 3 | `GfxEngine` core | Framebuffer alloc, clear, flush, setPixel, hLine |
| 4 | `fillRect` | Status bar background |
| 5 | `fillRoundedRect` | **Eye rendering** — validate with `normal-steady` |
| 6 | `fillCircle` | Pupils, dots |
| 7 | `drawLine` | Rain drops, dividers |
| 8 | Transform stack | Rotating sun rays, shake effects |
| 9 | Alpha blending | Opacity fades, breathing effects |
| 10 | `drawArc`, `drawBezier` | Happy arcs, heart paths, WiFi icon |
| 11 | `fillPolygon` | Sad eye lids |
| 12 | `drawText` + fonts | Scene labels, temperature, "READY" stamp |
| 13 | `strokeCircle`, `strokeRoundedRect` | Sweep ring, progress bar outlines |
| 14 | `drawDashedLine` | Boot checklist dotted leaders |
| 15 | DisplayManager refactor | Dual-mode rendering |
| 16 | StatusBar renderer | Fixed cyan bar with time + icons |

After step 5, the `normal-steady` variant (two blinking rounded rects)
should render correctly — this is the first end-to-end validation point.
