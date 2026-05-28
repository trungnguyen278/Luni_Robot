#pragma once
#include <functional>
#include <vector>
#include <mutex>
#include "StateTypes.hpp"

class StateManager {
public:
    using ConnectionCb   = std::function<void(state::ConnectionState, state::ConnectFailReason)>;
    using InteractionCb  = std::function<void(state::InteractionState, state::InputSource)>;
    using OtaCb          = std::function<void(state::OtaState)>;
    using SystemCb       = std::function<void(state::SystemState)>;
    using EmotionCb      = std::function<void(state::EmotionState)>;
    using StateEventCb   = std::function<void(const state::StateEvent&)>;

    static StateManager& instance();

    // Connection state (replaces old ConnectivityState)
    bool setConnectionState(state::ConnectionState s,
                            state::ConnectFailReason reason = state::ConnectFailReason::NONE);
    state::ConnectionState  getConnectionState();
    state::ConnectFailReason getLastFailReason();

    // Interaction state
    bool setInteractionState(state::InteractionState s,
                             state::InputSource src = state::InputSource::UNKNOWN);
    state::InteractionState  getInteractionState();
    state::InputSource       getInteractionSource();

    // OTA state
    bool setOtaState(state::OtaState s);
    state::OtaState getOtaState();

    // System state
    void setSystemState(state::SystemState s);
    state::SystemState getSystemState();

    // Emotion state
    void setEmotionState(state::EmotionState s);
    state::EmotionState getEmotionState();

    // Subscribe (returns subscription id)
    int subscribeConnection(ConnectionCb cb);
    int subscribeInteraction(InteractionCb cb);
    int subscribeOta(OtaCb cb);
    int subscribeSystem(SystemCb cb);
    int subscribeEmotion(EmotionCb cb);
    int subscribeAll(StateEventCb cb);

    // Unsubscribe
    void unsubscribeConnection(int id);
    void unsubscribeInteraction(int id);
    void unsubscribeOta(int id);
    void unsubscribeSystem(int id);
    void unsubscribeEmotion(int id);
    void unsubscribeAll(int id);

private:
    StateManager() = default;
    StateManager(const StateManager&) = delete;
    StateManager& operator=(const StateManager&) = delete;

    void notifyEvent(const state::StateEvent& event);

    std::mutex mtx_;

    state::ConnectionState   conn_state_    = state::ConnectionState::OFFLINE;
    state::ConnectFailReason fail_reason_   = state::ConnectFailReason::NONE;
    state::InteractionState  inter_state_   = state::InteractionState::IDLE;
    state::InputSource       inter_source_  = state::InputSource::UNKNOWN;
    state::OtaState          ota_state_     = state::OtaState::IDLE;
    state::SystemState       sys_state_     = state::SystemState::BOOTING;
    state::EmotionState      emo_state_     = state::EmotionState::NEUTRAL;

    int next_sub_id_ = 1;
    std::vector<std::pair<int, ConnectionCb>>   conn_cbs_;
    std::vector<std::pair<int, InteractionCb>>  inter_cbs_;
    std::vector<std::pair<int, OtaCb>>          ota_cbs_;
    std::vector<std::pair<int, SystemCb>>       sys_cbs_;
    std::vector<std::pair<int, EmotionCb>>      emo_cbs_;
    std::vector<std::pair<int, StateEventCb>>   all_cbs_;
};
