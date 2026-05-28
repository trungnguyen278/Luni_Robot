#include "ui/VariantRegistry.hpp"
#include "display/GfxEngine.hpp"
#include "display/MathHelpers.hpp"
#include <cmath>

using namespace geom;
using namespace math;

// Shared stub renders — placeholder animations for new categories.
// Each will be replaced with proper JSX-matched renders.

static void stub_eyes(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = blink(t, 0.5f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

static void stub_eyes_bob(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dy = sinf(t * TAU) * 3.0f;
    float b = blink(t, 0.7f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
}

static void stub_eyes_narrow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float b = blink(t, 0.6f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H * 0.6f, 4.0f, b);
    gfx.drawEye(LX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

static void stub_eyes_wide(GfxEngine& gfx, float t, const ColorContext& colors) {
    float s = 1.0f + sinf(t * TAU) * 0.08f;
    int16_t w = (int16_t)(EYE_W * s);
    int16_t h = (int16_t)(EYE_H * s);
    gfx.drawEye(LX, CY, w, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, w, h, EYE_RX, 0, colors.eye);
}

static void stub_eyes_tilt(GfxEngine& gfx, float t, const ColorContext& colors) {
    float tilt = sinf(t * TAU) * 5.0f;
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, tilt, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, EYE_H, EYE_RX, -tilt, colors.eye);
}

static void stub_eyes_sway(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dx = sinf(t * TAU) * 6.0f;
    float b = blink(t, 0.4f, 0.06f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye((int16_t)(LX + dx), CY, EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye((int16_t)(RX + dx), CY, EYE_W, h, EYE_RX, 0, colors.eye);
}

// --- greet (4) ---
static void render_greet_bow(GfxEngine& gfx, float t, const ColorContext& colors) {
    float dy = sinf(t * PI) * 12.0f;
    float b = blink(t, 0.3f, 0.07f);
    int16_t h = (int16_t)lerp((float)EYE_H, 4.0f, b);
    gfx.drawEye(LX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, (int16_t)(CY + dy), EYE_W, h, EYE_RX, 0, colors.eye);
}
const VariantDef GREET_VARIANTS[] = {
    {"greet-bow",        "Bow",        2800, TONE_NONE, render_greet_bow},
    {"greet-wave",       "Wave",       2600, TONE_NONE, stub_eyes_bob},
    {"greet-nod-twice",  "Nod twice",  2400, TONE_NONE, stub_eyes},
    {"greet-sparkle-hi", "Sparkle hi", 2800, TONE_NONE, stub_eyes_wide},
};
extern const CategoryDef CAT_GREET = {
    "greet", "Greet", ContentKind::EMOTION, TONE_CYAN,
    GREET_VARIANTS, sizeof(GREET_VARIANTS) / sizeof(GREET_VARIANTS[0])
};

// --- wink (4) ---
static void render_wink_right(GfxEngine& gfx, float t, const ColorContext& colors) {
    float wk = pulse(t, 0.4f, 0.15f);
    int16_t rh = (int16_t)lerp((float)EYE_H, 4.0f, wk);
    gfx.drawEye(LX, CY, EYE_W, EYE_H, EYE_RX, 0, colors.eye);
    gfx.drawEye(RX, CY, EYE_W, rh, EYE_RX, 0, colors.eye);
}
const VariantDef WINK_VARIANTS[] = {
    {"wink-right",  "Right",  2400, TONE_NONE, render_wink_right},
    {"wink-left",   "Left",   2400, TONE_NONE, stub_eyes},
    {"wink-double", "Double", 2800, TONE_NONE, stub_eyes},
    {"wink-slow",   "Slow",   3200, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_WINK = {
    "wink", "Wink", ContentKind::EMOTION, TONE_WARM,
    WINK_VARIANTS, sizeof(WINK_VARIANTS) / sizeof(WINK_VARIANTS[0])
};

// --- annoyed (3) ---
const VariantDef ANNOYED_VARIANTS[] = {
    {"annoyed-flat",       "Flat",       2600, TONE_NONE, stub_eyes_narrow},
    {"annoyed-twitch",     "Twitch",     2800, TONE_NONE, stub_eyes},
    {"annoyed-sigh-side",  "Sigh side",  3000, TONE_NONE, stub_eyes_sway},
};
extern const CategoryDef CAT_ANNOYED = {
    "annoyed", "Annoyed", ContentKind::EMOTION, TONE_ORANGE,
    ANNOYED_VARIANTS, sizeof(ANNOYED_VARIANTS) / sizeof(ANNOYED_VARIANTS[0])
};

// --- disgusted (3) ---
const VariantDef DISGUSTED_VARIANTS[] = {
    {"disgusted-yuck", "Yuck",  2400, TONE_NONE, stub_eyes_narrow},
    {"disgusted-gag",  "Gag",   2600, TONE_NONE, stub_eyes},
    {"disgusted-sour", "Sour",  2800, TONE_NONE, stub_eyes_narrow},
};
extern const CategoryDef CAT_DISGUSTED = {
    "disgusted", "Disgusted", ContentKind::EMOTION, TONE_GREEN,
    DISGUSTED_VARIANTS, sizeof(DISGUSTED_VARIANTS) / sizeof(DISGUSTED_VARIANTS[0])
};

// --- scared (4) ---
const VariantDef SCARED_VARIANTS[] = {
    {"scared-tremble", "Tremble", 2400, TONE_NONE, stub_eyes_wide},
    {"scared-shrink",  "Shrink",  2600, TONE_NONE, stub_eyes},
    {"scared-flinch",  "Flinch",  2200, TONE_NONE, stub_eyes_wide},
    {"scared-darting", "Darting", 2800, TONE_NONE, stub_eyes_sway},
};
extern const CategoryDef CAT_SCARED = {
    "scared", "Scared", ContentKind::EMOTION, TONE_WHITE,
    SCARED_VARIANTS, sizeof(SCARED_VARIANTS) / sizeof(SCARED_VARIANTS[0])
};

// --- nervous (3) ---
const VariantDef NERVOUS_VARIANTS[] = {
    {"nervous-sweat",  "Sweat",  2800, TONE_NONE, stub_eyes},
    {"nervous-fidget", "Fidget", 2600, TONE_NONE, stub_eyes_sway},
    {"nervous-gulp",   "Gulp",   2400, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_NERVOUS = {
    "nervous", "Nervous", ContentKind::EMOTION, TONE_ORANGE,
    NERVOUS_VARIANTS, sizeof(NERVOUS_VARIANTS) / sizeof(NERVOUS_VARIANTS[0])
};

// --- shy (4) ---
const VariantDef SHY_VARIANTS[] = {
    {"shy-away",  "Away",  2800, TONE_NONE, stub_eyes_sway},
    {"shy-down",  "Down",  2600, TONE_NONE, stub_eyes_bob},
    {"shy-peek",  "Peek",  3000, TONE_NONE, stub_eyes_narrow},
    {"shy-blush", "Blush", 2800, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_SHY = {
    "shy", "Shy", ContentKind::EMOTION, TONE_ROSE,
    SHY_VARIANTS, sizeof(SHY_VARIANTS) / sizeof(SHY_VARIANTS[0])
};

// --- embarrassed (3) ---
const VariantDef EMBARRASSED_VARIANTS[] = {
    {"embarrassed-blush",  "Blush",  2800, TONE_NONE, stub_eyes},
    {"embarrassed-hide",   "Hide",   3000, TONE_NONE, stub_eyes_narrow},
    {"embarrassed-cringe", "Cringe", 2600, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_EMBARRASSED = {
    "embarrassed", "Embarrassed", ContentKind::EMOTION, TONE_ROSE,
    EMBARRASSED_VARIANTS, sizeof(EMBARRASSED_VARIANTS) / sizeof(EMBARRASSED_VARIANTS[0])
};

// --- smug (4) ---
const VariantDef SMUG_VARIANTS[] = {
    {"smug-smirk", "Smirk", 2800, TONE_NONE, stub_eyes_narrow},
    {"smug-side",  "Side",  2600, TONE_NONE, stub_eyes_sway},
    {"smug-brow",  "Brow",  2800, TONE_NONE, stub_eyes_tilt},
    {"smug-blink", "Blink", 2400, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_SMUG = {
    "smug", "Smug", ContentKind::EMOTION, TONE_WARM,
    SMUG_VARIANTS, sizeof(SMUG_VARIANTS) / sizeof(SMUG_VARIANTS[0])
};

// --- proud (4) ---
const VariantDef PROUD_VARIANTS[] = {
    {"proud-puffed", "Puffed", 2800, TONE_NONE, stub_eyes_wide},
    {"proud-glow",   "Glow",   3000, TONE_NONE, stub_eyes},
    {"proud-medal",  "Medal",  3200, TONE_NONE, stub_eyes},
    {"proud-rise",   "Rise",   2600, TONE_NONE, stub_eyes_bob},
};
extern const CategoryDef CAT_PROUD = {
    "proud", "Proud", ContentKind::EMOTION, TONE_WARM,
    PROUD_VARIANTS, sizeof(PROUD_VARIANTS) / sizeof(PROUD_VARIANTS[0])
};

// --- cool (3) ---
const VariantDef COOL_VARIANTS[] = {
    {"cool-shades", "Shades", 3000, TONE_NONE, stub_eyes_narrow},
    {"cool-smirk",  "Smirk",  2800, TONE_NONE, stub_eyes_narrow},
    {"cool-swag",   "Swag",   3200, TONE_NONE, stub_eyes_sway},
};
extern const CategoryDef CAT_COOL = {
    "cool", "Cool", ContentKind::EMOTION, TONE_CYAN,
    COOL_VARIANTS, sizeof(COOL_VARIANTS) / sizeof(COOL_VARIANTS[0])
};

// --- mischievous (4) ---
const VariantDef MISCHIEVOUS_VARIANTS[] = {
    {"mischievous-grin",    "Grin",    2800, TONE_NONE, stub_eyes_narrow},
    {"mischievous-laugh",   "Laugh",   3000, TONE_NONE, stub_eyes},
    {"mischievous-side",    "Side",    2600, TONE_NONE, stub_eyes_sway},
    {"mischievous-twinkle", "Twinkle", 2800, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_MISCHIEVOUS = {
    "mischievous", "Mischievous", ContentKind::EMOTION, TONE_PURPLE,
    MISCHIEVOUS_VARIANTS, sizeof(MISCHIEVOUS_VARIANTS) / sizeof(MISCHIEVOUS_VARIANTS[0])
};

// --- suspicious (3) ---
const VariantDef SUSPICIOUS_VARIANTS[] = {
    {"suspicious-squint", "Squint", 2800, TONE_NONE, stub_eyes_narrow},
    {"suspicious-side",   "Side",   2600, TONE_NONE, stub_eyes_sway},
    {"suspicious-scan",   "Scan",   3000, TONE_NONE, stub_eyes_sway},
};
extern const CategoryDef CAT_SUSPICIOUS = {
    "suspicious", "Suspicious", ContentKind::EMOTION, TONE_PURPLE,
    SUSPICIOUS_VARIANTS, sizeof(SUSPICIOUS_VARIANTS) / sizeof(SUSPICIOUS_VARIANTS[0])
};

// --- curious (3) ---
const VariantDef CURIOUS_VARIANTS[] = {
    {"curious-lean",    "Lean",    2800, TONE_NONE, stub_eyes_tilt},
    {"curious-sparkle", "Sparkle", 3000, TONE_NONE, stub_eyes_wide},
    {"curious-scan",    "Scan",    2600, TONE_NONE, stub_eyes_sway},
};
extern const CategoryDef CAT_CURIOUS = {
    "curious", "Curious", ContentKind::EMOTION, TONE_ORANGE,
    CURIOUS_VARIANTS, sizeof(CURIOUS_VARIANTS) / sizeof(CURIOUS_VARIANTS[0])
};

// --- focused (4) ---
const VariantDef FOCUSED_VARIANTS[] = {
    {"focused-laser",    "Laser",    2800, TONE_NONE, stub_eyes_narrow},
    {"focused-scan",     "Scan",     3000, TONE_NONE, stub_eyes},
    {"focused-target",   "Target",   2600, TONE_NONE, stub_eyes_narrow},
    {"focused-pinpoint", "Pinpoint", 2400, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_FOCUSED = {
    "focused", "Focused", ContentKind::EMOTION, TONE_RED,
    FOCUSED_VARIANTS, sizeof(FOCUSED_VARIANTS) / sizeof(FOCUSED_VARIANTS[0])
};

// --- determined (3) ---
const VariantDef DETERMINED_VARIANTS[] = {
    {"determined-stare", "Stare", 2800, TONE_NONE, stub_eyes_narrow},
    {"determined-fire",  "Fire",  3000, TONE_NONE, stub_eyes},
    {"determined-lock",  "Lock",  2600, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_DETERMINED = {
    "determined", "Determined", ContentKind::EMOTION, TONE_RED,
    DETERMINED_VARIANTS, sizeof(DETERMINED_VARIANTS) / sizeof(DETERMINED_VARIANTS[0])
};

// --- charging (4) ---
const VariantDef CHARGING_VARIANTS[] = {
    {"charging-bolt",  "Bolt",  2800, TONE_NONE, stub_eyes},
    {"charging-fill",  "Fill",  3000, TONE_NONE, stub_eyes_bob},
    {"charging-spark", "Spark", 2600, TONE_NONE, stub_eyes},
    {"charging-glow",  "Glow",  3200, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_CHARGING = {
    "charging", "Charging", ContentKind::EMOTION, TONE_GREEN,
    CHARGING_VARIANTS, sizeof(CHARGING_VARIANTS) / sizeof(CHARGING_VARIANTS[0])
};

// --- error (4) ---
const VariantDef ERROR_VARIANTS[] = {
    {"error-bang",    "Bang",    2400, TONE_NONE, stub_eyes_wide},
    {"error-noconn",  "No conn", 2800, TONE_NONE, stub_eyes},
    {"error-pixels",  "Pixels",  3000, TONE_NONE, stub_eyes},
    {"error-warning", "Warning", 2600, TONE_NONE, stub_eyes_wide},
};
extern const CategoryDef CAT_ERROR = {
    "error", "Error", ContentKind::EMOTION, TONE_RED,
    ERROR_VARIANTS, sizeof(ERROR_VARIANTS) / sizeof(ERROR_VARIANTS[0])
};

// --- mute (4) ---
const VariantDef MUTE_VARIANTS[] = {
    {"mute-x",    "X",    2400, TONE_NONE, stub_eyes},
    {"mute-zip",  "Zip",  2600, TONE_NONE, stub_eyes},
    {"mute-hush", "Hush", 2800, TONE_NONE, stub_eyes_narrow},
    {"mute-line", "Line", 2400, TONE_NONE, stub_eyes},
};
extern const CategoryDef CAT_MUTE = {
    "mute", "Mute", ContentKind::EMOTION, TONE_CYAN,
    MUTE_VARIANTS, sizeof(MUTE_VARIANTS) / sizeof(MUTE_VARIANTS[0])
};

// --- hungry (4) ---
const VariantDef HUNGRY_VARIANTS[] = {
    {"hungry-eye-food", "Eye food", 2800, TONE_NONE, stub_eyes_wide},
    {"hungry-drool",    "Drool",    3000, TONE_NONE, stub_eyes},
    {"hungry-lick",     "Lick",     2600, TONE_NONE, stub_eyes},
    {"hungry-stomach",  "Stomach",  2800, TONE_NONE, stub_eyes_bob},
};
extern const CategoryDef CAT_HUNGRY = {
    "hungry", "Hungry", ContentKind::EMOTION, TONE_ORANGE,
    HUNGRY_VARIANTS, sizeof(HUNGRY_VARIANTS) / sizeof(HUNGRY_VARIANTS[0])
};
