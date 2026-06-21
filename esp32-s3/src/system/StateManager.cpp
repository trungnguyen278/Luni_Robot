#include "StateManager.hpp"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "StateManager";

StateManager& StateManager::instance()
{
    static StateManager inst;
    return inst;
}

// === Setters ===

void StateManager::setInteractionState(state::InteractionState s, state::InputSource src)
{
    decltype(interaction_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (s == interaction_state && src == interaction_source) return;
        interaction_state = s;
        interaction_source = src;
        cbs = interaction_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s, src);
}

void StateManager::setConnectionState(state::ConnectionState s,
                                      state::ConnectFailReason reason)
{
    decltype(connectivity_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        fail_reason = reason;
        if (s == connectivity_state) return;
        connectivity_state = s;
        cbs = connectivity_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s);
}

void StateManager::setSystemState(state::SystemState s)
{
    decltype(system_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (s == system_state) return;
        system_state = s;
        cbs = system_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s);
}

void StateManager::setPowerState(state::PowerState s)
{
    decltype(power_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (s == power_state) return;
        power_state = s;
        cbs = power_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s);
}

void StateManager::setEmotionState(state::EmotionState s)
{
    decltype(emotion_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (s == emotion_state) return;
        emotion_state = s;
        cbs = emotion_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s);
}

void StateManager::setOtaState(state::OtaState s)
{
    decltype(ota_cbs) cbs;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (s == ota_state) return;
        ota_state = s;
        cbs = ota_cbs;
    }
    for (auto& [id, cb] : cbs) cb(s);
}

// === Getters ===

state::InteractionState  StateManager::getInteractionState()  { std::lock_guard<std::mutex> lock(mtx); return interaction_state; }
state::InputSource       StateManager::getInteractionSource() { std::lock_guard<std::mutex> lock(mtx); return interaction_source; }
state::ConnectionState StateManager::getConnectionState()  { std::lock_guard<std::mutex> lock(mtx); return connectivity_state; }
state::ConnectFailReason StateManager::getLastFailReason()   { std::lock_guard<std::mutex> lock(mtx); return fail_reason; }
state::SystemState       StateManager::getSystemState()        { std::lock_guard<std::mutex> lock(mtx); return system_state; }
state::PowerState        StateManager::getPowerState()         { std::lock_guard<std::mutex> lock(mtx); return power_state; }
state::EmotionState      StateManager::getEmotionState()       { std::lock_guard<std::mutex> lock(mtx); return emotion_state; }
state::OtaState          StateManager::getOtaState()           { std::lock_guard<std::mutex> lock(mtx); return ota_state; }

// === Subscribe ===

int StateManager::subscribeInteraction(InteractionCb cb)  { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; interaction_cbs.push_back({id, std::move(cb)}); return id; }
int StateManager::subscribeConnectivity(ConnectivityCb cb) { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; connectivity_cbs.push_back({id, std::move(cb)}); return id; }
int StateManager::subscribeSystem(SystemCb cb)             { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; system_cbs.push_back({id, std::move(cb)}); return id; }
int StateManager::subscribePower(PowerCb cb)               { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; power_cbs.push_back({id, std::move(cb)}); return id; }
int StateManager::subscribeEmotion(EmotionCb cb)           { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; emotion_cbs.push_back({id, std::move(cb)}); return id; }
int StateManager::subscribeOta(OtaCb cb)                   { std::lock_guard<std::mutex> lock(mtx); int id = next_sub_id++; ota_cbs.push_back({id, std::move(cb)}); return id; }

// === Unsubscribe ===

void StateManager::unsubscribeInteraction(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    interaction_cbs.erase(std::remove_if(interaction_cbs.begin(), interaction_cbs.end(),
        [id](auto& p){ return p.first == id; }), interaction_cbs.end());
}
void StateManager::unsubscribeConnectivity(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    connectivity_cbs.erase(std::remove_if(connectivity_cbs.begin(), connectivity_cbs.end(),
        [id](auto& p){ return p.first == id; }), connectivity_cbs.end());
}
void StateManager::unsubscribeSystem(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    system_cbs.erase(std::remove_if(system_cbs.begin(), system_cbs.end(),
        [id](auto& p){ return p.first == id; }), system_cbs.end());
}
void StateManager::unsubscribePower(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    power_cbs.erase(std::remove_if(power_cbs.begin(), power_cbs.end(),
        [id](auto& p){ return p.first == id; }), power_cbs.end());
}
void StateManager::unsubscribeEmotion(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    emotion_cbs.erase(std::remove_if(emotion_cbs.begin(), emotion_cbs.end(),
        [id](auto& p){ return p.first == id; }), emotion_cbs.end());
}
void StateManager::unsubscribeOta(int id) {
    std::lock_guard<std::mutex> lock(mtx);
    ota_cbs.erase(std::remove_if(ota_cbs.begin(), ota_cbs.end(),
        [id](auto& p){ return p.first == id; }), ota_cbs.end());
}
