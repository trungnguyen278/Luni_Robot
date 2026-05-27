# Claude Design to Embedded C++ — Porting Pipeline

Guide for converting Luni UI designs (JSX/SVG) into procedural C++ render
functions on the ESP32-S3.

---

## 1. Design System Overview

All UI designs live in `ui_design/`. They are React JSX components that
render SVG to a virtual 320x240 viewport.

### File layout and load order

| # | File | Purpose |
|---|------|---------|
| 1 | `emotion-core.jsx` | Screen geometry, math helpers, SVG primitives (`Eye`, `Eyes`, `heartPath`, `starPath`, `arcPath`, `lidPath`), color palette (`EYE_TONES`) |
| 2 | `emotion-list.jsx` | 29 base emotion categories (normal, happy, sad, etc.) |
| 3 | `emotion-extras.jsx` | Additional variants for existing categories |
| 4 | `emotion-balance.jsx` | Balance pass — variant count adjustments |
| 5 | `emotion-more.jsx` | 8 new emotion categories (disgusted, nervous, etc.) |
| 6 | `scenes.jsx` | 8 base scene categories (weather, clock, music, etc.) |
| 7 | `scenes-extras.jsx` | 11 additional scene categories |
| 8 | `scenes-boot.jsx` | Boot sequence (4 variants) |
| 9 | `scenes-network.jsx` | Network status (6 variants) |
| 10 | `emotion-color.jsx` | Tone assignment for all categories and per-variant overrides |
| 11 | `app.jsx` | Interactive viewer UI |

### Core concept: variant render function

Every variant is a pure function of normalized time:

```javascript
// JSX
{ id: 'normal-steady', duration: 5200,
  render: (t) => {                    // t ∈ [0, 1), cycles every duration ms
    const b = blink(t, 0.32, 0.07);
    const h = lerp(EYE_H, 4, b);
    return <Eyes L={{ h }} R={{ h }} />;
  }
}
```

The C++ equivalent is:

```cpp
void render_normal_steady(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(math::blink(t, 0.32f, 0.07f), math::blink(t, 0.78f, 0.07f));
    float h = math::lerp(EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, (int16_t)h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, (int16_t)h, EYE_RX, 0, colors.eye);
}
```

### Two kinds of content

| Kind | Eyes rendered? | Color rule | Full screen? |
|------|---------------|------------|-------------|
| `emotion` | Yes | Face = cyan, accessories = category tone | No (320x220, below status bar) |
| `scene` | No | Everything = category/variant tone | Yes (320x220, below status bar) |

---

## 2. JSX Primitive to C++ Mapping

### SVG Elements

| JSX/SVG | Parameters | C++ GfxEngine | Notes |
|---------|-----------|---------------|-------|
| `<rect>` with `rx` | x, y, width, height, rx, fill, opacity | `gfx.fillRoundedRect(x, y, w, h, rx, color, alpha)` | **Most used** — every Eye shape |
| `<rect>` no rx | x, y, width, height, fill, opacity | `gfx.fillRect(x, y, w, h, color, alpha)` | Progress bars, rectangles |
| `<circle>` | cx, cy, r, fill | `gfx.fillCircle(cx, cy, r, color, alpha)` | Pupils, dots, snowflakes |
| `<circle>` stroke only | cx, cy, r, stroke, strokeWidth | `gfx.strokeCircle(cx, cy, r, color, strokeWidth)` | Sweep rings, pulse waves |
| `<ellipse>` | cx, cy, rx, ry, fill | `gfx.fillEllipse(cx, cy, rx, ry, color, alpha)` | Cloud shapes |
| `<line>` | x1, y1, x2, y2, stroke, strokeWidth | `gfx.drawLine(x1, y1, x2, y2, color, width, alpha)` | Rain drops, sun rays, dividers |
| `<line>` dashed | + strokeDasharray="2 3" | `gfx.drawDashedLine(x1, y1, x2, y2, color, w, dash, gap)` | Boot checklist dotted leaders |
| `<path>` Q (quadratic) | M..Q..endpoint | `gfx.drawQuadBezier(x0, y0, cx, cy, x1, y1, color, w)` | Happy mouth arcs, wifi arcs |
| `<path>` C (cubic) | M..C..endpoint | `gfx.drawCubicBezier(x0, y0, cx1, cy1, cx2, cy2, x1, y1, color, w)` | Heart shape, Luni laurel |
| `<path>` A (arc) | M..A..endpoint | `gfx.drawArc(cx, cy, r, startAngle, endAngle, color, w)` | Sweep ring on boot-logo |
| `<path>` polyline | M..L..L..Z | `gfx.fillPolygon(xPts, yPts, count, color)` | Sad eye lids, check marks |
| `<text>` | x, y, fontSize, fill, textAnchor | `gfx.drawText(text, x, y, font, color, align, alpha)` | Scene labels, temperature |

### Transform Container Elements

| JSX | C++ |
|-----|-----|
| `<g transform="translate(dx dy)">` | `gfx.pushTransform(); gfx.translate(dx, dy);` ... `gfx.popTransform();` |
| `<g transform="rotate(angle cx cy)">` | `gfx.pushTransform(); gfx.rotate(angle, cx, cy);` ... `gfx.popTransform();` |
| `<g transform="scale(s)">` | `gfx.pushTransform(); gfx.scale(s, s);` ... `gfx.popTransform();` |
| `<g opacity={0.5}>` | `gfx.pushAlpha(128);` ... `gfx.popAlpha();` |

### CSS Variable Color References

| JSX CSS Variable | ColorContext Field | Description |
|-----------------|-------------------|-------------|
| `var(--eye)` | `colors.eye` | Face color — cyan for emotions, tone for scenes |
| `var(--accent)` | `colors.accent` | Accessory color — category tone |
| `var(--bg)` | `colors.bg` | Background — always near-black (#06090d) |
| `var(--tone-warm)` | `colors.tones[TONE_WARM]` | Named tone for multi-color variants |
| `var(--tone-rose)` | `colors.tones[TONE_ROSE]` | Named tone for multi-color variants |
| (all 9 tones) | `colors.tones[...]` | Available for `tone: 'multi'` variants |

---

## 3. Color System Porting

### 3.1 EYE_TONES Palette — RGB565 Conversion

Source: `emotion-core.jsx` lines 104-113.

| Tone | Hex (24-bit) | RGB565 | R5 | G6 | B5 | Usage |
|------|-------------|--------|----|----|-----|-------|
| cyan | `#5be9ff` | `0x5F9F` | 11 | 58 | 31 | Default, neutral, listening, thinking |
| warm | `#ffd166` | `0xFE8C` | 31 | 52 | 12 | Happy, excited, smug, proud |
| rose | `#ff6b9d` | `0xFB53` | 31 | 26 | 19 | Love, shy, embarrassed |
| red | `#ff5b6e` | `0xFACE` | 31 | 22 | 14 | Angry, error, dead, focused |
| blue | `#76b8ff` | `0x75DF` | 14 | 46 | 31 | Sad, crying |
| green | `#7be88e` | `0x7F51` | 15 | 58 | 17 | Charging, disgusted |
| purple | `#b48cff` | `0xB47F` | 22 | 36 | 31 | Sleepy, mischievous, dizzy |
| orange | `#ff9d5b` | `0xFCEB` | 31 | 39 | 11 | Hungry, confused, curious |
| white | `#f0f4ff` | `0xF7BF` | 30 | 61 | 31 | Surprised, scared |
| bg | `#06090d` | `0x0041` | 0 | 2 | 1 | Screen background |

RGB565 conversion formula:
```
R5 = (R8 >> 3) & 0x1F
G6 = (G8 >> 2) & 0x3F
B5 = (B8 >> 3) & 0x1F
rgb565 = (R5 << 11) | (G6 << 5) | B5
```

### 3.2 Two-Layer Color Rule

**Emotions** (kind = 'emotion'):
- `--eye` = always cyan → `colors.eye = TONE_CYAN`
- `--accent` = category tone → `colors.accent = TONES[resolvedTone]`
- Face shapes (eyes, mouth, brows) use `colors.eye`
- Accessories (hearts, sparkles, tears, Zs) use `colors.accent`

**Scenes** (kind = 'scene'):
- `--eye` = category/variant tone → `colors.eye = TONES[resolvedTone]`
- `--accent` = same tone → `colors.accent = TONES[resolvedTone]`
- Everything uses the resolved tone color

**Multi-color** (tone = 'multi'):
- Default `--eye` and `--accent` = cyan
- Variant explicitly references `var(--tone-warm)`, `var(--tone-rose)`, etc.
- All 9 tones available via `colors.tones[]`

### 3.3 Tone Resolution Order

```
resolvedTone = variant.tone || category.tone || 'cyan'
```

Complete tone map is in `emotion-color.jsx`:
- `EMOTION_TONE`: 34 emotion → tone mappings (lines 18-73)
- `SCENE_TONE`: 21 scene → tone mappings (lines 76-98)
- `SCENE_VARIANT_TONE`: 47 per-variant overrides (lines 102-203)

---

## 4. Math Helpers Porting

Source: `emotion-core.jsx` lines 20-38.

### C++ equivalents

```cpp
// MathHelpers.hpp
namespace math {

constexpr float TAU = 6.2831853f;

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float wrap(float v, float m) {
    float r = fmodf(v, m);
    return r < 0 ? r + m : r;
}

namespace ease {
    inline float inOut(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2) / 2.0f;
    }
    inline float out(float t) { return 1.0f - powf(1.0f - t, 3); }
    inline float in(float t) { return t * t * t; }
}

inline float pulse(float t, float center, float w) {
    float d = fabsf(t - center);
    if (d > w) return 0.0f;
    float c = cosf((d / w) * (M_PI / 2.0f));
    return c * c;
}

inline float blink(float t, float at, float d = 0.06f) {
    float dd = t - at;
    if (dd < 0 || dd > d) return 0.0f;
    return sinf((dd / d) * M_PI);
}

} // namespace math
```

---

## 5. Screen Geometry Constants

Source: `emotion-core.jsx` lines 6-17.

```cpp
// Constants matching JSX viewport
constexpr int16_t SCREEN_W  = 320;
constexpr int16_t SCREEN_H  = 240;
constexpr int16_t STATUS_H  = 20;
constexpr int16_t CY        = STATUS_H + (SCREEN_H - STATUS_H) / 2; // 130
constexpr int16_t GAP       = 150;
constexpr int16_t LX        = SCREEN_W / 2 - GAP / 2;  // 85
constexpr int16_t RX        = SCREEN_W / 2 + GAP / 2;  // 235
constexpr int16_t EYE_W     = 88;
constexpr int16_t EYE_H     = 96;
constexpr int16_t EYE_RX    = 22;
constexpr int16_t SCX       = SCREEN_W / 2;  // 160 (scene center X)
constexpr int16_t SCY       = CY;            // 130 (scene center Y)
```

---

## 6. Step-by-Step: Porting a New Variant

### Step 1: Identify the variant in JSX

Open the appropriate JSX file (e.g., `emotion-list.jsx` for `normal-steady`).
Find the variant entry:

```javascript
{ id: 'normal-steady', label: 'Steady', duration: 5200,
  render: (t) => {
    const b = Math.max(blink(t, 0.32, 0.07), blink(t, 0.78, 0.07));
    const h = lerp(EYE_H, 4, b);
    return baseEyes({ h }, { h });
  }
}
```

### Step 2: List all dependencies

For this variant:
- Primitives: `Eyes` → two `fillRoundedRect` calls
- Math: `blink`, `lerp`
- Colors: `var(--eye)` only (no accessories)
- Transforms: none
- Text: none

### Step 3: Write the C++ render function

```cpp
// File: esp32-s3/src/ui/emotions/render_normal.cpp

#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"

void render_normal_steady(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = fmaxf(math::blink(t, 0.32f, 0.07f), math::blink(t, 0.78f, 0.07f));
    int16_t h = (int16_t)math::lerp(EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}
```

### Step 4: Register the variant

```cpp
// In the category registry
static const VariantDef normal_variants[] = {
    { "normal-steady", "Steady", 5200, TONE_NONE, render_normal_steady },
    // ... more variants
};

static const CategoryDef cat_normal = {
    "normal", "Normal", ContentKind::EMOTION, TONE_CYAN,
    normal_variants, sizeof(normal_variants) / sizeof(normal_variants[0])
};
```

### Step 5: Verify against JSX viewer

1. Open `MOCHI-P Emotion Sheet.html` in a browser
2. Navigate to the category/variant
3. Compare rendering at key time values: t=0, 0.25, 0.5, 0.75
4. Check color correctness (face cyan, accessories in tone)
5. Check animation timing matches `duration` field

---

## 7. Porting Complex Examples

### Example: Weather-Sunny (scene with transforms, text, and loops)

JSX source (`scenes.jsx`):
```javascript
render: (t) => {
  const cx = SCX - 60, cy = CY - 10;
  return (
    <g>
      {/* Sun rays rotating */}
      <g transform={`translate(${cx} ${cy}) rotate(${t * 360 * 0.5})`}>
        {Array.from({ length: 8 }).map((_, i) => {
          const a = (i / 8) * TAU;
          return <line x1={cos(a)*36} y1={sin(a)*36}
                       x2={cos(a)*52} y2={sin(a)*52}
                       stroke="var(--eye)" strokeWidth={4} />;
        })}
      </g>
      <circle cx={cx} cy={cy} r={28} fill="var(--eye)" />
      <circle cx={cx} cy={cy} r={20} fill="var(--bg)" />
      {bigText({ x: SCX+60, y: cy-4, fontSize: 36 }, '28°')}
      {bigText({ x: SCX+60, y: cy+26, fontSize: 12, opacity: 0.7 }, 'SUNNY')}
    </g>
  );
}
```

C++ port:
```cpp
void render_weather_sunny(GfxEngine& gfx, float t, const ColorContext& colors) {
    const int16_t cx = SCX - 60, cy = CY - 10;

    // Rotating sun rays
    gfx.pushTransform();
    gfx.translate(cx, cy);
    gfx.rotate(t * 180.0f, 0, 0);  // 0.5 revolution per cycle
    for (int i = 0; i < 8; i++) {
        float a = (float)i / 8.0f * math::TAU;
        float ca = cosf(a), sa = sinf(a);
        gfx.drawLine((int16_t)(ca * 36), (int16_t)(sa * 36),
                      (int16_t)(ca * 52), (int16_t)(sa * 52),
                      colors.eye, 4);
    }
    gfx.popTransform();

    // Sun body (ring = filled circle + bg circle)
    gfx.fillCircle(cx, cy, 28, colors.eye);
    gfx.fillCircle(cx, cy, 20, colors.bg);

    // Temperature text (data-driven in real implementation)
    auto& data = SceneManager::instance().getSceneData();
    char temp[8];
    snprintf(temp, sizeof(temp), "%d\xC2\xB0", data.weather_temp_c);
    gfx.drawText(temp, SCX + 60, cy - 4, FONT_36, colors.eye, TextAlign::CENTER);
    gfx.drawText("SUNNY", SCX + 60, cy + 26, FONT_12, colors.eye, TextAlign::CENTER, 178);
}
```

### Example: Boot-Poweron (multi-phase animation)

This variant has 4 phases controlled by `t`:
- 0.00-0.20: dot grows
- 0.20-0.50: stretches into horizontal bar
- 0.50-0.80: bar splits into two eye blocks
- 0.80-1.00: settles to default eye proportions

Each phase uses `lerp` + `ease.out`/`ease.inOut` to interpolate dimensions.
Port each `if/else` block as-is — the math translates directly.

### Example: Network-BLE-Pair (reusable glyph components)

The JSX defines helper components (`WifiIcon`, `BTIcon`, `CloudIcon`) that
are reused across variants. Port these as standalone C++ functions:

```cpp
void drawWifiIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                  float scale, int bars, uint16_t color, uint8_t alpha = 255);

void drawBTIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                float scale, uint16_t color, uint8_t alpha = 255);

void drawCloudIcon(GfxEngine& gfx, int16_t cx, int16_t cy,
                   float scale, uint16_t color, uint8_t alpha = 255);
```

Place in `esp32-s3/src/ui/scenes/scene_glyphs.hpp` for reuse.

---

## 8. Status Bar Rendering

The status bar is rendered every frame, independent of the active variant.
Source: `app.jsx` lines 87-117.

```
Layout (y=0 to y=20, always cyan, never tinted):
  [8, 14]    Time text "14:32"        (11px monospace, cyan, 85% opacity)
  [160, 10]  Center dot or label      (r=2 circle, cyan, 70% opacity)
  [274, 6]   WiFi icon (3 arcs)       (1.2px stroke, cyan, 85% opacity)
  [292, 5]   Battery icon             (rect outline + fill, cyan, 85% opacity)
  [0, 20]    Separator line           (0.5px, cyan, 25% opacity)
```

The status bar always uses `TONE_CYAN` regardless of active category.

---

## 9. Existing Asset Pipeline (icons/logos only)

The `convert_assets.py` script in `esp32-c5/scripts/` converts PNG/GIF to
2-bit RLE C++ arrays. This pipeline is **only for icons and logos** — small,
static, monochrome assets.

```bash
# Icon
python scripts/convert_assets.py icon battery.png src/assets/ --width 20

# Logo (GIF animation)
python scripts/convert_assets.py emotion logo.gif src/assets/ --fps 20 --loop
```

All emotions and scenes use **procedural C++ rendering** instead. See
`RENDERING_ARCHITECTURE.md` for the GfxEngine design.

---

## 10. File Organization

```
esp32-s3/
├── lib/display/
│   ├── GfxEngine.hpp/.cpp       # Core 2D graphics engine
│   ├── ColorSystem.hpp          # Tone LUT, ColorContext
│   ├── MathHelpers.hpp          # lerp, ease, pulse, blink
│   ├── DisplayDriver.hpp/.cpp   # ST7789 SPI driver (existing)
│   └── AnimationPlayer.hpp/.cpp # Legacy 1-bit RLE (kept for migration)
│
├── src/ui/
│   ├── VariantRegistry.hpp/.cpp # Category/variant registry
│   ├── SceneManager.hpp/.cpp    # Scene lifecycle, data injection
│   ├── StatusBar.hpp/.cpp       # Status bar renderer
│   ├── emotions/
│   │   ├── render_normal.cpp    # normal category (16 variants)
│   │   ├── render_happy.cpp     # happy category (8 variants)
│   │   ├── render_sad.cpp       # sad category (6 variants)
│   │   └── ...                  # one file per category
│   └── scenes/
│       ├── scene_glyphs.hpp     # Shared glyph components (WiFi, BT, Cloud)
│       ├── render_boot.cpp      # boot category (4 variants)
│       ├── render_network.cpp   # network category (6 variants)
│       ├── render_weather.cpp   # weather category (5 variants)
│       └── ...                  # one file per scene category
```

---

## 11. Porting Priority Order

### Phase 1: Core system + validation
1. `normal` (16 variants) — validates GfxEngine basics (rounded rect, blink)
2. `happy` (8 variants) — validates arcs, sparkles
3. `sad` (6 variants) — validates lids (polygon masks), tears

### Phase 2: Interaction states
4. `listening` (7 variants) — validates audio wave accessories
5. `thinking` (7 variants) — validates animation loops
6. `loading` (3 variants)

### Phase 3: Boot + connectivity (dLunice-critical)
7. `boot` (4 variants) — validates multi-phase animation, text, progress bar
8. `network` (6 variants) — validates glyph components (WiFi, BT, Cloud)

### Phase 4: Scenes with real-time data
9. `weather` (5 variants) — validates data-driven rendering
10. `clock` (3 variants) — validates NTP integration
11. Remaining scene categories

### Phase 5: Remaining emotions
12-37. All other emotion categories in alphabetical order.
