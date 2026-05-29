#include "SceneManager.hpp"
#include <cstring>
#include <cstdlib>
#include "esp_log.h"

static const char* TAG = "SceneManager";

static SceneManager s_instance;

SceneManager& SceneManager::instance() { return s_instance; }

void SceneManager::showScene(const char* categoryKey, const char* variantId) {
    const CategoryDef* cat = findCategory(categoryKey);
    if (!cat || cat->variant_count == 0) return;

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
    if (!variant) return;

    current_category_ = cat;
    current_variant_ = variant;
    elapsed_ms_ = 0;
    scene_active_ = (cat->kind != ContentKind::EMOTION);
    idle_active_ = false;
    ESP_LOGI(TAG, "Show %s/%s (%ums)",
             cat->key, variant->id, variant->duration_ms);
}

void SceneManager::showEmotion(const char* categoryKey, const char* variantId) {
    showScene(categoryKey, variantId);
    scene_active_ = false;
}

void SceneManager::exitScene() {
    scene_active_ = false;
    idle_active_ = true;
    idle_timer_ms_ = 0;
    idle_interval_ms_ = 3000.0f + (float)(rand() % 3000);
}

const VariantDef* SceneManager::pickVariant(const char* categoryKey) {
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
        if (isCategoryRecent(cat.key)) continue;
        candidates[count++] = &cat;
    }

    if (count == 0) {
        for (uint8_t i = 0; i < CATEGORY_COUNT && count < 64; i++) {
            const auto& cat = ALL_CATEGORIES[i];
            if (isIdleExcluded(cat.key)) continue;
            if (cat.kind == ContentKind::STATUS) continue;
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

    if (!should_loop && raw >= 1.0f) {
        exitScene();
    }
}
