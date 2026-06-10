#pragma once
#include <cstdint>
#include <cstring>
#include "display/GfxEngine.hpp"
#include "display/ColorSystem.hpp"
#include "display/MathHelpers.hpp"
#include "ui/VariantRegistry.hpp"

struct SceneData {
    // Time (from server sync via C5)
    uint8_t  hours = 0;
    uint8_t  minutes = 0;
    uint8_t  seconds = 0;
    uint8_t  day_of_week = 0;
    uint8_t  day = 1;
    uint8_t  month = 1;
    uint16_t year = 2026;
    bool     time_valid = false;
    int64_t  unix_time = 0;
    int8_t   utc_offset = 0;

    // Weather (from server sync via C5)
    int8_t   temperature = 0;
    uint8_t  humidity = 0;
    uint8_t  weather_condition = 0;  // enum: 0=unknown, 1=clear, ...
    uint8_t  aqi = 0;
    bool     weather_valid = false;

    // Calendar (from server sync via C5)
    uint8_t  lunar_day = 0;
    uint8_t  lunar_month = 0;

    // Location (from server sync via C5)
    char     city[32] = {};

    // Boot
    uint8_t  boot_progress_pct = 0;
    uint8_t  boot_check_results[5] = {};

    // Network
    char     ssid[33] = {};
    uint8_t  retry_attempt = 0;
    uint8_t  retry_max = 3;
    uint16_t error_code = 0;

    // BLE
    char     ble_pin[8] = {};

    // Local sensors (S3)
    uint8_t  battery_percent = 0;
    bool     is_charging = false;
    int8_t   wifi_rssi = 0;
};

class SceneManager {
public:
    SceneManager() = default;

    // Hold-window sentinels for showScene/showEmotion `hold_ms`:
    //   HOLD_AUTO   → window picked from content kind (one play for a scene,
    //                 DEFAULT_EMOTION_HOLD_MS for an emotion), then return to idle.
    //   HOLD_STICKY → stay until the caller explicitly swaps it out. Use for
    //                 interaction-bound faces (listening/thinking/error) whose
    //                 lifetime is owned by a state, not a timer.
    //   >= 0        → explicit window in milliseconds.
    static constexpr float HOLD_AUTO   = -1.0f;
    static constexpr float HOLD_STICKY = -2.0f;
    static constexpr float DEFAULT_EMOTION_HOLD_MS = 6000.0f;

    void showScene(const char* categoryKey, const char* variantId = nullptr,
                   float hold_ms = HOLD_AUTO);
    void showEmotion(const char* categoryKey, const char* variantId = nullptr,
                     float hold_ms = HOLD_AUTO);
    void exitScene();

    bool isSceneActive() const { return scene_active_; }

    void updateSceneData(const SceneData& data) { scene_data_ = data; }
    SceneData& getSceneData() { return scene_data_; }
    const SceneData& getSceneData() const { return scene_data_; }

    // Call every frame with delta time in ms
    void update(GfxEngine& gfx, float dt_ms);

    // Pick random variant from a category (with recency filter).
    // The `moon` category is special-cased: it never picks randomly — the
    // variant is the one matching tonight's lunar day (see pickMoonVariant).
    const VariantDef* pickVariant(const char* categoryKey);

    // Idle loop management
    void tickIdle(float dt_ms);
    bool isIdle() const { return !scene_active_ && idle_active_; }

    // Current state
    const VariantDef* currentVariant() const { return current_variant_; }
    const CategoryDef* currentCategory() const { return current_category_; }

    static SceneManager& instance();

private:
    ColorContext resolveColors() const;

    const VariantDef* current_variant_ = nullptr;
    const CategoryDef* current_category_ = nullptr;
    float elapsed_ms_ = 0;
    bool scene_active_ = false;
    bool idle_active_ = true;

    // Explicitly shown content (showScene/showEmotion) with a finite lifetime:
    // when `holding_`, update() returns to the idle rotation once elapsed_ms_
    // reaches `hold_ms_`. Idle-driven content leaves these false.
    bool  holding_ = false;
    float hold_ms_ = 0.0f;

    // Recency filter (variants)
    static constexpr int RECENT_SIZE = 6;
    const char* recent_ids_[RECENT_SIZE] = {};
    int recent_head_ = 0;

    // Recency filter (categories for idle)
    static constexpr int RECENT_CAT_SIZE = 8;
    const char* recent_cats_[RECENT_CAT_SIZE] = {};
    int recent_cat_head_ = 0;
    bool isCategoryRecent(const char* key) const;
    void recordCategory(const char* key);

    // Idle timer
    float idle_timer_ms_ = 0;
    float idle_interval_ms_ = 4000;
    const CategoryDef* pickIdleCategory();

    // Deterministic moon-phase variant for tonight's lunar day (from
    // scene_data_.lunar_day, fed by the server). Mirrors the JSX
    // LuniMoon.sceneForLunarDay() mapping.
    const VariantDef* pickMoonVariant();

    SceneData scene_data_;
};
