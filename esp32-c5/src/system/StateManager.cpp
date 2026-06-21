#include "StateManager.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>

static const char* TAG = "StateManager";

StateManager& StateManager::instance()
{
    static StateManager inst;
    return inst;
}

void StateManager::notifyEvent(const state::StateEvent& event)
{
    std::vector<std::pair<int, StateEventCb>> cbs;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        cbs = all_cbs_;
    }
    for (auto& p : cbs) { if (p.second) p.second(event); }
}

// === Connection ===

bool StateManager::setConnectionState(state::ConnectionState s, state::ConnectFailReason reason)
{
    std::vector<std::pair<int, ConnectionCb>> callbacks;
    state::ConnectionState old_state;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (s == conn_state_) return true;

        if (!state::isValidConnectionTransition(conn_state_, s)) {
            ESP_LOGW(TAG, "Invalid connection transition: %d -> %d", (int)conn_state_, (int)s);
            return false;
        }

        old_state = conn_state_;
        ESP_LOGI(TAG, "Connection: %d -> %d (reason=%d)", (int)conn_state_, (int)s, (int)reason);
        conn_state_ = s;
        fail_reason_ = reason;
        callbacks = conn_cbs_;
    }

    for (auto& p : callbacks) { if (p.second) p.second(s, reason); }

    state::StateEvent event{};
    event.type = state::StateEvent::Type::CONNECTION;
    event.old_value = (uint8_t)old_state;
    event.new_value = (uint8_t)s;
    event.fail_reason = reason;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    notifyEvent(event);

    return true;
}

state::ConnectionState StateManager::getConnectionState()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return conn_state_;
}

state::ConnectFailReason StateManager::getLastFailReason()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return fail_reason_;
}

int StateManager::subscribeConnection(ConnectionCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    conn_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeConnection(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    conn_cbs_.erase(
        std::remove_if(conn_cbs_.begin(), conn_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        conn_cbs_.end());
}

// === Interaction ===

bool StateManager::setInteractionState(state::InteractionState s, state::InputSource src)
{
    std::vector<std::pair<int, InteractionCb>> callbacks;
    state::InteractionState old_state;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (s == inter_state_ && src == inter_source_) return true;

        // Soft validation: an off-contract transition is a bug somewhere, but
        // rejecting it would desync the relay to S3 — warn and apply anyway.
        if (s != inter_state_ && !state::isValidInteractionTransition(inter_state_, s)) {
            ESP_LOGW(TAG, "Off-contract interaction transition: %d -> %d (src=%d)",
                     (int)inter_state_, (int)s, (int)src);
        }

        old_state = inter_state_;
        ESP_LOGI(TAG, "Interaction: %d -> %d (source=%d)", (int)inter_state_, (int)s, (int)src);
        inter_state_ = s;
        inter_source_ = src;
        callbacks = inter_cbs_;
    }

    for (auto& p : callbacks) { if (p.second) p.second(s, src); }

    state::StateEvent event{};
    event.type = state::StateEvent::Type::INTERACTION;
    event.old_value = (uint8_t)old_state;
    event.new_value = (uint8_t)s;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    notifyEvent(event);

    return true;
}

state::InteractionState StateManager::getInteractionState()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return inter_state_;
}

state::InputSource StateManager::getInteractionSource()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return inter_source_;
}

int StateManager::subscribeInteraction(InteractionCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    inter_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeInteraction(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    inter_cbs_.erase(
        std::remove_if(inter_cbs_.begin(), inter_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        inter_cbs_.end());
}

// === OTA ===

bool StateManager::setOtaState(state::OtaState s)
{
    std::vector<std::pair<int, OtaCb>> callbacks;
    state::OtaState old_state;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (s == ota_state_) return true;

        if (!state::isValidOtaTransition(ota_state_, s)) {
            ESP_LOGW(TAG, "Invalid OTA transition: %d -> %d", (int)ota_state_, (int)s);
            return false;
        }

        old_state = ota_state_;
        ESP_LOGI(TAG, "OTA: %d -> %d", (int)ota_state_, (int)s);
        ota_state_ = s;
        callbacks = ota_cbs_;
    }

    for (auto& p : callbacks) { if (p.second) p.second(s); }

    state::StateEvent event{};
    event.type = state::StateEvent::Type::OTA;
    event.old_value = (uint8_t)old_state;
    event.new_value = (uint8_t)s;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    notifyEvent(event);

    return true;
}

state::OtaState StateManager::getOtaState()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return ota_state_;
}

int StateManager::subscribeOta(OtaCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    ota_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeOta(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    ota_cbs_.erase(
        std::remove_if(ota_cbs_.begin(), ota_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        ota_cbs_.end());
}

// === System ===

void StateManager::setSystemState(state::SystemState s)
{
    std::vector<std::pair<int, SystemCb>> callbacks;
    state::SystemState old_state;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        if (s == sys_state_) return;
        old_state = sys_state_;
        ESP_LOGI(TAG, "System: %d -> %d", (int)sys_state_, (int)s);
        sys_state_ = s;
        callbacks = sys_cbs_;
    }

    for (auto& p : callbacks) { if (p.second) p.second(s); }

    state::StateEvent event{};
    event.type = state::StateEvent::Type::SYSTEM;
    event.old_value = (uint8_t)old_state;
    event.new_value = (uint8_t)s;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    notifyEvent(event);
}

state::SystemState StateManager::getSystemState()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return sys_state_;
}

int StateManager::subscribeSystem(SystemCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    sys_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeSystem(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    sys_cbs_.erase(
        std::remove_if(sys_cbs_.begin(), sys_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        sys_cbs_.end());
}

// === Emotion ===

void StateManager::setEmotionState(state::EmotionState s)
{
    std::vector<std::pair<int, EmotionCb>> callbacks;
    state::EmotionState old_state;
    {
        std::lock_guard<std::mutex> lk(mtx_);
        old_state = emo_state_;
        emo_state_ = s;
        ESP_LOGI(TAG, "Emotion: %d -> %d", (int)old_state, (int)s);
        callbacks = emo_cbs_;
    }

    for (auto& p : callbacks) { if (p.second) p.second(s); }

    state::StateEvent event{};
    event.type = state::StateEvent::Type::EMOTION;
    event.old_value = (uint8_t)old_state;
    event.new_value = (uint8_t)s;
    event.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    notifyEvent(event);
}

state::EmotionState StateManager::getEmotionState()
{
    std::lock_guard<std::mutex> lk(mtx_);
    return emo_state_;
}

int StateManager::subscribeEmotion(EmotionCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    emo_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeEmotion(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    emo_cbs_.erase(
        std::remove_if(emo_cbs_.begin(), emo_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        emo_cbs_.end());
}

// === All events ===

int StateManager::subscribeAll(StateEventCb cb)
{
    std::lock_guard<std::mutex> lk(mtx_);
    int id = next_sub_id_++;
    all_cbs_.emplace_back(id, std::move(cb));
    return id;
}

void StateManager::unsubscribeAll(int id)
{
    std::lock_guard<std::mutex> lk(mtx_);
    all_cbs_.erase(
        std::remove_if(all_cbs_.begin(), all_cbs_.end(),
            [id](auto& p) { return p.first == id; }),
        all_cbs_.end());
}
