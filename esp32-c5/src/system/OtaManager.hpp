#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system/StateTypes.hpp"

class StateManager;

class OtaManager {
public:
    OtaManager() = default;
    ~OtaManager() = default;

    void init(StateManager& sm);

    void checkUpdate(const char* current_version, const char* model);
    void startDownload(const char* url, const char* sha256, size_t size);
    void cancel();

    state::OtaState getState() const { return state_; }
    uint8_t getProgress() const { return progress_; }

private:
    static void downloadTaskEntry(void* arg);
    void downloadTask();

    StateManager* sm_ = nullptr;
    std::atomic<state::OtaState> state_{state::OtaState::IDLE};
    std::atomic<uint8_t> progress_{0};

    struct OtaInfo {
        char url[256];
        char sha256[65];
        size_t size;
    } pending_{};

    std::atomic<bool> cancel_requested_{false};
    TaskHandle_t dl_task_ = nullptr;
};
