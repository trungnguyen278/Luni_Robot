#include "SceneManager.hpp"
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "esp_log.h"

static const char* TAG = "SceneManager";

static SceneManager s_instance;

SceneManager& SceneManager::instance() { return s_instance; }

void SceneManager::showScene(const char* categoryKey, const char* variantId,
                             float hold_ms) {
    const CategoryDef* cat = findCategory(categoryKey);
    if (!cat || cat->variant_count == 0) {
        // Previously a silent return — a server/app set_scene (or set_emotion)
        // with a key that has no asset just vanished with no trace, making
        // "robot face didn't change" impossible to debug (R-DA-RBT-006).
        ESP_LOGW(TAG, "showScene: unknown/empty category '%s' — nothing rendered",
                 categoryKey ? categoryKey : "(null)");
        return;
    }

    const VariantDef* variant = nullptr;
    if (variantId) {
        for (uint8_t i = 0; i < cat->variant_count; i++) {
            if (strcmp(cat->variants[i].id, variantId) == 0) {
                variant = &cat->variants[i];
                break;
            }
        }
    }
    if (!variant) variant = pickVariant(categoryKey);
    if (!variant) {
        ESP_LOGW(TAG, "showScene: no variant for '%s'/'%s' — nothing rendered",
                 cat->key, variantId ? variantId : "(auto)");
        return;
    }

    current_category_ = cat;
    current_variant_ = variant;
    elapsed_ms_ = 0;
    scene_active_ = (cat->kind != ContentKind::EMOTION);
    idle_active_ = false;

    // Resolve the hold window. Explicitly shown content no longer stays forever
    // ("set cố định") — once the window elapses, update() returns to the idle
    // rotation. STICKY content is left to the caller to swap out (state-bound).
    if (hold_ms == HOLD_STICKY) {
        holding_ = false;
        hold_ms_ = 0.0f;
    } else {
        hold_ms_ = (hold_ms == HOLD_AUTO)
                       ? (cat->kind == ContentKind::EMOTION
                              ? DEFAULT_EMOTION_HOLD_MS
                              : (float)variant->duration_ms)
                       : hold_ms;
        holding_ = true;
    }

    ESP_LOGI(TAG, "Show %s/%s (%ums, hold=%dms)",
             cat->key, variant->id, variant->duration_ms,
             holding_ ? (int)hold_ms_ : -1);
}

void SceneManager::showEmotion(const char* categoryKey, const char* variantId,
                               float hold_ms) {
    showScene(categoryKey, variantId, hold_ms);
    scene_active_ = false;
}

void SceneManager::exitScene() {
    scene_active_ = false;
    idle_active_ = true;
    holding_ = false;
    idle_timer_ms_ = 0;
    idle_interval_ms_ = 3000.0f + (float)(rand() % 3000);

    // Each scene ends on its own duration — don't freeze on its last frame for
    // the whole idle gap. Drop to the (looping, animated) resting face until
    // tickIdle rotates in the next idle animation.
    const CategoryDef* normal = findCategory("normal");
    if (normal && normal->variant_count > 0) {
        current_category_ = normal;
        current_variant_  = pickVariant("normal");
        elapsed_ms_ = 0;
    }
}

const VariantDef* SceneManager::pickVariant(const char* categoryKey) {
    // The moon is never random — it always shows tonight's lunar phase.
    // This covers both idle (tickIdle) and explicit showScene("moon").
    if (strcmp(categoryKey, "moon") == 0) {
        const VariantDef* mv = pickMoonVariant();
        if (mv) return mv;
    }

    // Weather is never random either — it shows the actual synced condition.
    // (Falls through to random only when unsynced / unknown.) (S11-02)
    if (strcmp(categoryKey, "weather") == 0) {
        const VariantDef* wv = pickWeatherVariant();
        if (wv) return wv;
    }

    const CategoryDef* cat = findCategory(categoryKey);
    if (!cat || cat->variant_count == 0) return nullptr;

    // Filter out recently played
    const VariantDef* candidates[32];
    int count = 0;
    for (uint8_t i = 0; i < cat->variant_count && count < 32; i++) {
        bool recent = false;
        for (int r = 0; r < RECENT_SIZE; r++) {
            if (recent_ids_[r] && strcmp(recent_ids_[r], cat->variants[i].id) == 0) {
                recent = true;
                break;
            }
        }
        if (!recent) candidates[count++] = &cat->variants[i];
    }

    // Fall back to full list if all filtered
    if (count == 0) {
        for (uint8_t i = 0; i < cat->variant_count && count < 32; i++) {
            candidates[count++] = &cat->variants[i];
        }
    }

    const VariantDef* pick = candidates[rand() % count];

    // Record in recency buffer
    recent_ids_[recent_head_] = pick->id;
    recent_head_ = (recent_head_ + 1) % RECENT_SIZE;

    return pick;
}

// Nearest of the 8 canonical phases (i/8) to a synodic position p∈[0,1),
// measured circularly. Mirrors sceneIdForP() in ui_design/scenes-moon.jsx.
static int moonIndexForP(float p) {
    p -= floorf(p);
    int best = 0;
    float bestd = 9.0f;
    for (int i = 0; i < 8; i++) {
        float d = fabsf((float)i / 8.0f - p);
        if (d > 0.5f) d = 1.0f - d;
        if (d < bestd) { bestd = d; best = i; }
    }
    return best;
}

const VariantDef* SceneManager::pickMoonVariant() {
    const CategoryDef* cat = findCategory("moon");
    if (!cat || cat->variant_count == 0) return nullptr;

    constexpr float SYNODIC = 29.530588853f;
    int idx;

    if (scene_data_.lunar_day >= 1 && scene_data_.lunar_day <= 30) {
        // Server-provided âm lịch day (sent on connect / day change).
        float p = (float)((scene_data_.lunar_day - 1) % 30) / SYNODIC;
        idx = moonIndexForP(p);
    } else if (scene_data_.unix_time > 0) {
        // Fallback: compute phase from the synced clock (port of moonPhaseP).
        // Reference new moon: 2000-01-06 18:14 UTC = 10962.7597 days since epoch.
        const double NEWREF_DAYS = 10962.7597;
        double days = (double)scene_data_.unix_time / 86400.0;
        double age = fmod(days - NEWREF_DAYS, (double)SYNODIC);
        if (age < 0) age += SYNODIC;
        idx = moonIndexForP((float)(age / SYNODIC));
    } else {
        idx = 0;  // not synced yet → Sóc
    }

    return &cat->variants[idx % cat->variant_count];
}

const VariantDef* SceneManager::pickWeatherVariant() {
    const CategoryDef* cat = findCategory("weather");
    if (!cat || cat->variant_count == 0) return nullptr;

    // Map the server weather-condition enum (DataSyncManager::conditionToEnum)
    // to a weather variant id. 0 / unknown → nullptr so the caller can fall
    // back to a random pick (REQUIREMENTS §6.7: show the real sky).
    const char* id = nullptr;
    switch (scene_data_.weather_condition) {
        case 1:  id = "weather-window"; break;  // clear / sunny
        case 2:                                  // partly cloudy
        case 3:  id = "weather-cloudy"; break;  // cloudy
        case 4:                                  // rain
        case 5:  id = "weather-rainy";  break;  // heavy rain
        case 6:  id = "weather-storm";  break;  // thunderstorm
        case 7:  id = "weather-snow";   break;  // snow
        case 8:                                  // fog
        case 10: id = "weather-fog";    break;  // haze
        case 9:  id = "weather-windy";  break;  // windy
        default: return nullptr;                 // unknown / not synced
    }

    for (uint8_t i = 0; i < cat->variant_count; i++) {
        if (strcmp(cat->variants[i].id, id) == 0) return &cat->variants[i];
    }
    return nullptr;
}

bool SceneManager::isCategoryRecent(const char* key) const {
    for (int i = 0; i < RECENT_CAT_SIZE; i++) {
        if (recent_cats_[i] && strcmp(recent_cats_[i], key) == 0)
            return true;
    }
    return false;
}

void SceneManager::recordCategory(const char* key) {
    recent_cats_[recent_cat_head_] = key;
    recent_cat_head_ = (recent_cat_head_ + 1) % RECENT_CAT_SIZE;
}

static bool isIdleExcluded(const char* key) {
    static const char* EXCLUDED[] = {
        "thinking", "listening", "loading", "boot", "network",
        "charging", "error", "mute",
    };
    for (auto* ex : EXCLUDED) {
        if (strcmp(key, ex) == 0) return true;
    }
    return false;
}

const CategoryDef* SceneManager::pickIdleCategory() {
    const CategoryDef* candidates[64];
    int count = 0;

    for (uint8_t i = 0; i < CATEGORY_COUNT && count < 64; i++) {
        const auto& cat = ALL_CATEGORIES[i];
        if (isIdleExcluded(cat.key)) continue;
        if (cat.kind == ContentKind::STATUS) continue;
        // REQUIREMENTS §9: scenes (clock/weather/moon/call/alarm…) are picked
        // EXPLICITLY by the host on a real event — they never enter the random
        // idle pool, where they'd show fake/empty data (S11-04).
        if (cat.kind == ContentKind::SCENE) continue;
        if (isCategoryRecent(cat.key)) continue;
        candidates[count++] = &cat;
    }

    if (count == 0) {
        for (uint8_t i = 0; i < CATEGORY_COUNT && count < 64; i++) {
            const auto& cat = ALL_CATEGORIES[i];
            if (isIdleExcluded(cat.key)) continue;
            if (cat.kind == ContentKind::STATUS) continue;
            if (cat.kind == ContentKind::SCENE) continue;
            candidates[count++] = &cat;
        }
    }
    if (count == 0) return findCategory("normal");
    return candidates[rand() % count];
}

void SceneManager::tickIdle(float dt_ms) {
    if (scene_active_ || !idle_active_) return;

    idle_timer_ms_ += dt_ms;
    if (idle_timer_ms_ >= idle_interval_ms_) {
        idle_timer_ms_ = 0;

        const CategoryDef* cat = pickIdleCategory();
        if (!cat) return;

        const VariantDef* v = pickVariant(cat->key);
        if (!v) return;

        recordCategory(cat->key);
        current_category_ = cat;
        current_variant_ = v;
        elapsed_ms_ = 0;

        if (cat->kind == ContentKind::SCENE) {
            if (cat->loops) {
                // Looping scenes (weather, music): play one cycle then return to idle
                scene_active_ = false;
                idle_active_ = true;
                idle_interval_ms_ = (float)v->duration_ms + 500.0f;
            } else {
                scene_active_ = true;
                idle_active_ = false;
                idle_interval_ms_ = 3000.0f + (float)(rand() % 3000);
            }
        } else {
            idle_interval_ms_ = 3000.0f + (float)(rand() % 5000);
        }

        ESP_LOGD(TAG, "Idle: %s/%s", cat->key, v->id);
    }
}

ColorContext SceneManager::resolveColors() const {
    if (!current_category_ || !current_variant_) {
        return ColorContext::forEmotion(TONE_CYAN);
    }
    ToneId tone = resolveTone(current_category_, current_variant_);
    if (current_category_->kind == ContentKind::SCENE) {
        return ColorContext::forScene(tone);
    }
    return ColorContext::forEmotion(tone);
}

void SceneManager::update(GfxEngine& gfx, float dt_ms) {
    if (!current_variant_ || !current_variant_->render) return;

    elapsed_ms_ += dt_ms;
    float dur = (float)current_variant_->duration_ms;
    float raw = (dur > 0) ? elapsed_ms_ / dur : 0.0f;
    bool should_loop;
    if (!current_category_) {
        should_loop = true;
    } else if (current_category_->kind == ContentKind::EMOTION) {
        should_loop = true;
    } else if (current_category_->kind == ContentKind::SCENE) {
        should_loop = false;
    } else {
        should_loop = current_category_->loops;
    }
    float t = should_loop ? fmodf(raw, 1.0f) : fminf(raw, 0.999f);

    ColorContext colors = resolveColors();
    current_variant_->render(gfx, t, colors);

    // Return to the idle rotation when an explicitly held scene/emotion has run
    // out its window (emotions used to loop forever). Idle-driven content is not
    // `holding_`; non-looping idle scenes still fall back on the raw>=1 finish.
    if (holding_) {
        if (elapsed_ms_ >= hold_ms_) exitScene();
    } else if (!should_loop && raw >= 1.0f) {
        exitScene();
    }
}
