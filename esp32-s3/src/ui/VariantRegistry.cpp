#include "VariantRegistry.hpp"
#include <cstring>

// Emotions (37 base + 10 Luni packs = 47 categories)
extern const CategoryDef CAT_NORMAL;
extern const CategoryDef CAT_GREET;
extern const CategoryDef CAT_HAPPY;
extern const CategoryDef CAT_WINK;
extern const CategoryDef CAT_SAD;
extern const CategoryDef CAT_CRYING;
extern const CategoryDef CAT_ANGRY;
extern const CategoryDef CAT_ANNOYED;
extern const CategoryDef CAT_DISGUSTED;
extern const CategoryDef CAT_SURPRISED;
extern const CategoryDef CAT_SCARED;
extern const CategoryDef CAT_NERVOUS;
extern const CategoryDef CAT_LOVE;
extern const CategoryDef CAT_SHY;
extern const CategoryDef CAT_EMBARRASSED;
extern const CategoryDef CAT_SMUG;
extern const CategoryDef CAT_PROUD;
extern const CategoryDef CAT_COOL;
extern const CategoryDef CAT_MISCHIEVOUS;
extern const CategoryDef CAT_SUSPICIOUS;
extern const CategoryDef CAT_CURIOUS;
extern const CategoryDef CAT_CONFUSED;
extern const CategoryDef CAT_SLEEPY;
extern const CategoryDef CAT_SLEEPING;
extern const CategoryDef CAT_EXCITED;
extern const CategoryDef CAT_BORED;
extern const CategoryDef CAT_HUNGRY;
extern const CategoryDef CAT_LISTENING;
extern const CategoryDef CAT_THINKING;
extern const CategoryDef CAT_FOCUSED;
extern const CategoryDef CAT_DETERMINED;
extern const CategoryDef CAT_LOADING;
extern const CategoryDef CAT_CHARGING;
extern const CategoryDef CAT_DIZZY;
extern const CategoryDef CAT_DEAD;
extern const CategoryDef CAT_ERROR;
extern const CategoryDef CAT_MUTE;
// Luni emotion packs
extern const CategoryDef CAT_SICK;
extern const CategoryDef CAT_COLD;
extern const CategoryDef CAT_CALM;
extern const CategoryDef CAT_PLAYFUL;
extern const CategoryDef CAT_HOT;
extern const CategoryDef CAT_LONELY;
extern const CategoryDef CAT_GRATEFUL;
extern const CategoryDef CAT_BRAVE;
extern const CategoryDef CAT_DREAMY;
extern const CategoryDef CAT_AWE;

// Status flows
extern const CategoryDef CAT_BOOT;
extern const CategoryDef CAT_NETWORK;

// Scenes (32 categories)
extern const CategoryDef CAT_WEATHER;
extern const CategoryDef CAT_CLOCK;
extern const CategoryDef CAT_MUSIC;
extern const CategoryDef CAT_TIMER;
extern const CategoryDef CAT_COOKING;
extern const CategoryDef CAT_WALKING;
extern const CategoryDef CAT_CELEBRATE;
extern const CategoryDef CAT_NIGHT;
extern const CategoryDef CAT_FITNESS;
extern const CategoryDef CAT_CALL;
extern const CategoryDef CAT_MESSAGE;
extern const CategoryDef CAT_CAMERA;
extern const CategoryDef CAT_NAVIGATION;
extern const CategoryDef CAT_GIFT;
extern const CategoryDef CAT_COFFEE;
extern const CategoryDef CAT_PLANT;
extern const CategoryDef CAT_MORNING;
extern const CategoryDef CAT_GAMING;
extern const CategoryDef CAT_CALENDAR;
extern const CategoryDef CAT_ALARM;
extern const CategoryDef CAT_NOTIFICATION;
extern const CategoryDef CAT_BIRTHDAY;
extern const CategoryDef CAT_REMINDER;
extern const CategoryDef CAT_VIDEO;
extern const CategoryDef CAT_READING;
extern const CategoryDef CAT_SMARTHOME;
extern const CategoryDef CAT_SHOPPING;
extern const CategoryDef CAT_LOCK;
extern const CategoryDef CAT_DELIVERY;
extern const CategoryDef CAT_FLASHLIGHT;
extern const CategoryDef CAT_PODCAST;
extern const CategoryDef CAT_STOPWATCH;

// Authoritative order (matches ui_design/scenes-arc-pack-3b.jsx ORDER block)
const CategoryDef ALL_CATEGORIES[] = {
    // Emotions (37 base)
    CAT_NORMAL,
    CAT_GREET,
    CAT_HAPPY,
    CAT_WINK,
    CAT_SAD,
    CAT_CRYING,
    CAT_ANGRY,
    CAT_ANNOYED,
    CAT_DISGUSTED,
    CAT_SURPRISED,
    CAT_SCARED,
    CAT_NERVOUS,
    CAT_LOVE,
    CAT_SHY,
    CAT_EMBARRASSED,
    CAT_SMUG,
    CAT_PROUD,
    CAT_COOL,
    CAT_MISCHIEVOUS,
    CAT_SUSPICIOUS,
    CAT_SLEEPY,
    CAT_SLEEPING,
    CAT_EXCITED,
    CAT_CONFUSED,
    CAT_CURIOUS,
    CAT_BORED,
    CAT_HUNGRY,
    CAT_LISTENING,
    CAT_THINKING,
    CAT_FOCUSED,
    CAT_DETERMINED,
    CAT_LOADING,
    CAT_CHARGING,
    CAT_DIZZY,
    CAT_DEAD,
    CAT_ERROR,
    CAT_MUTE,
    // Status flows
    CAT_BOOT,
    CAT_NETWORK,
    // Luni emotion packs
    CAT_SICK,
    CAT_COLD,
    CAT_CALM,
    CAT_PLAYFUL,
    CAT_HOT,
    CAT_LONELY,
    CAT_GRATEFUL,
    CAT_BRAVE,
    CAT_DREAMY,
    CAT_AWE,
    // Scenes
    CAT_WEATHER,
    CAT_CLOCK,
    CAT_MUSIC,
    CAT_TIMER,
    CAT_COOKING,
    CAT_WALKING,
    CAT_CELEBRATE,
    CAT_NIGHT,
    CAT_FITNESS,
    CAT_CALL,
    CAT_MESSAGE,
    CAT_CAMERA,
    CAT_NAVIGATION,
    CAT_GIFT,
    CAT_COFFEE,
    CAT_PLANT,
    CAT_MORNING,
    CAT_GAMING,
    CAT_CALENDAR,
    CAT_ALARM,
    CAT_NOTIFICATION,
    CAT_BIRTHDAY,
    CAT_REMINDER,
    CAT_VIDEO,
    CAT_READING,
    CAT_SMARTHOME,
    CAT_SHOPPING,
    CAT_LOCK,
    CAT_DELIVERY,
    CAT_FLASHLIGHT,
    CAT_PODCAST,
    CAT_STOPWATCH,
};

const uint8_t CATEGORY_COUNT = sizeof(ALL_CATEGORIES) / sizeof(ALL_CATEGORIES[0]);

ToneId resolveTone(const CategoryDef* cat, const VariantDef* variant) {
    if (variant && variant->tone != TONE_NONE) return variant->tone;
    if (cat) return cat->tone;
    return TONE_CYAN;
}

const CategoryDef* findCategory(const char* key) {
    for (uint8_t i = 0; i < CATEGORY_COUNT; i++) {
        if (strcmp(ALL_CATEGORIES[i].key, key) == 0)
            return &ALL_CATEGORIES[i];
    }
    return nullptr;
}

const VariantDef* findVariant(const char* id) {
    for (uint8_t i = 0; i < CATEGORY_COUNT; i++) {
        const auto& cat = ALL_CATEGORIES[i];
        for (uint8_t v = 0; v < cat.variant_count; v++) {
            if (strcmp(cat.variants[v].id, id) == 0)
                return &cat.variants[v];
        }
    }
    return nullptr;
}
