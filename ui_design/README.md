# Luni Emotion Library — README

Luni is an original desktop-companion robot with a 320×240 screen that
shows **expressive eyes** and, when context calls for it, **full-screen
scenes** (weather, calls, music, timers…). This repo is the emotion +
scene + status library that drives that screen, plus an HTML viewer to
browse every state.

> Looking for the exact spec (geometry, palette, animation contract,
> porting checklist)? Read **`REQUIREMENTS.md`** — it's the canonical
> source of truth. This README is the quick start.

---

## Open it

Open **`Luni Emotion Sheet.html`** in a browser. No build step — it loads
React + Babel from a CDN and transpiles the `.jsx` files in place.

You'll see:

- **Stage** (left): the selected state playing at 3× on a device bezel.
- **Library** (right): pick a category, then a variant. Categories are
  grouped **Emotions · Scenes · Status**.
- **Actual-size sheet** (bottom): every category at native 320×240.

Controls: `← ⤬ →` step/randomise variants · **◐ Color / ◯ Mono** ·
**◉ Idle mode** (random-plays the `normal` pool like firmware idle) ·
**▶ Autoplay** · **fps** and **hold** sliders.

---

## The three kinds of content

| Kind        | What it is                                              | Picked by |
| ----------- | ------------------------------------------------------- | --------- |
| `emotion`   | Eyes-based expressions tied to mood / dialog            | random idle pool + dialog state |
| `scene`     | Full-screen contextual mini-videos                      | host, explicitly (`showScene`) |
| `status`    | System chrome: power-on `boot` + connectivity `network` | boot / connectivity FSM |

Totals: **81 categories · 248 variants** — 47 emotion · 32 scene · 2
status.

---

## Scenes: single-arc vs. multi-case

Every scene is a continuous **mini-video** (§6.6 in REQUIREMENTS): the
eyes shrink into a corner to "watch" a prop, then grow back. Most scenes
have **one** such arc.

But some scenes depict a **state of the world that varies**, so they
carry **one case per state** (§6.7). Each case is its own distinct
story — different props, motion, tone, eye emotion and caption:

| Scene     | Cases | Variants                                                              |
| --------- | ----- | --------------------------------------------------------------------- |
| `weather` | **7** | sunny · cloudy · rainy · storm · snow · fog · windy                   |
| `moon`    | **8** | trăng non/Sóc · lưỡi liềm đầu · thượng huyền · khuyết đầu · Rằm · khuyết cuối · hạ huyền · liềm tàn |
| `call`    | **3** | incoming · outgoing · missed                                          |
| `message` | **3** | chat · incoming · sent                                                |

The **host chooses the case** matching the live condition — these don't
enter the random idle pool. `moon` is the lunar version: **Luni *is* the
moon** — its own disc waxes and wanes and its mood shifts with the phase
(ngủ vùi at Sóc → rạng rỡ at Rằm → về nghỉ), so the host plays the story
for tonight's `ngày âm lịch` — `showScene('moon', LuniMoon.sceneId())` (or
`sceneForLunarDay(15)` for Rằm). The 30 lunar days fold onto the 8 phase
stories, so the robot lives a different mood per phase instead of only
dimming the screen like the app. Example:

```js
function showWeather(condition) {       // 'sunny' | 'rain' | 'snow' | ...
  const map = {
    clear: 'weather-window', sunny: 'weather-window',
    cloudy: 'weather-cloudy', rain: 'weather-rainy',
    storm: 'weather-storm', thunder: 'weather-storm',
    snow: 'weather-snow', fog: 'weather-fog', wind: 'weather-windy',
  };
  const id = map[condition] || 'weather-window';   // fallback: sunny
  play(EMOTION_CATEGORIES.weather.variants.find(v => v.id === id));
}
```

`call` and `message` work the same way (pick `call-missed`,
`message-sent`, etc.).

---

## Add a new weather (or call / message) case

1. Open `scenes-weather.jsx` (weather) or `scenes-comms.jsx`
   (call / message).
2. Add an entry to the `NEW_WEATHER` array (or a new `const` case for
   comms). Give it a unique kebab-case `id`, a `label`, a `tone`, a
   `duration`, and a `render: (t) => JSX` that is a **pure function of
   `t ∈ [0,1)`**.
3. For weather, use the `weatherCase({...})` factory — you only supply
   the opening/watch/closing emotions, the captions, and an
   `act(t, ph, watchP)` that draws the sky + props. The eye choreography
   (shrink-to-corner → watch → grow back) is shared.
4. Keep the new case **visually distinct** from the others — its own
   props and beat, not a recolour of an existing one.

Nothing else needs touching: these files only `push()` onto existing
categories, so no category key order or load order changes.

---

## Colour rules in one line

The **face** (eyes/brows/mouth) is **always cyan** — the robot's
identity. The category/variant **tone** only colours **accessories and
scene props** (`--accent`, and for scenes also `--eye`). The **status
bar** is fixed cyan, always. Toggle **◯ Mono** to verify nothing relies
on colour to read. Full detail in REQUIREMENTS §4.

---

## File map (load order)

```
Luni Emotion Sheet.html   entry point (CSS + script loader)
emotion-core.jsx          primitives, geometry, EYE_TONES palette
scene-arc.jsx             scene-arc engine + LuniFace
emotion-list / -extras    base emotion variants
scenes.jsx                tags pre-existing categories kind:'emotion'
scenes-boot / -network    status flows (boot, network)
emotion-balance / -more   top-ups + 8 more emotion categories
emotion-color.jsx         tone assignment
emotion-pack-luni{,2,3}   +10 Luni emotions
scenes-arc-pack-1..3b     the 32 scene arcs (+ authoritative KEYS order)
scenes-weather.jsx        ← multi-case weather (this work)
scenes-comms.jsx          ← multi-case call + message (this work)
scenes-moon.jsx           ← multi-case moon: one story per lunar phase
app.jsx                   viewer UI
```

Order matters: every category must be registered before tones and arc
overrides run. The two multi-case files load **after** the arc packs (so
the base categories exist) and **before** `app.jsx`.

---

## Porting to firmware

See **REQUIREMENTS.md §10 (porting checklist)** and §9 (random-picker
reference). In short: copy every `*.jsx` + `REQUIREMENTS.md`, replace the
SVG primitives with native draw calls, build a tone LUT from `EYE_TONES`,
honour the two-layer colour rule, implement `pickVariant` for idle and
`showScene` / `showWeather` for explicit scenes.
