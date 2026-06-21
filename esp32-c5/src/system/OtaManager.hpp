#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "system/StateTypes.hpp"

class StateManager;

class OtaManager {
public:
    using ProgressCb = std::function<void(uint8_t pct)>;

    OtaManager() = default;
    ~OtaManager() = default;

    void init(StateManager& sm);

    void checkUpdate(const char* current_version, const char* model);
    void startDownload(const char* url, const char* sha256, size_t size);
    void cancel();

    // Confirm the running image is healthy so the bootloader (rollback enabled)
    // stops watching it for revert. Safe no-op unless we booted a freshly-OTA'd
    // image that is still PENDING_VERIFY. Call once a strong health signal is
    // reached (we use WS auth success). Without this, a good OTA would roll back.
    void confirmHealth();

    // Fired on download progress steps (≥5% increments). State changes are
    // published via StateManager::setOtaState; this only carries the percent.
    void onProgress(ProgressCb cb) { progress_cb_ = std::move(cb); }

    state::OtaState getState() const { return state_; }
    uint8_t getProgress() const { return progress_; }

private:
    static void downloadTaskEntry(void* arg);
    void downloadTask();

    StateManager* sm_ = nullptr;
    std::atomic<state::OtaState> state_{state::OtaState::IDLE};
    std::atomic<uint8_t> progress_{0};
    ProgressCb progress_cb_;

    struct OtaInfo {
        char url[256];
        char sha256[65];
        size_t size;
    } pending_{};

    std::atomic<bool> cancel_requested_{false};
    TaskHandle_t dl_task_ = nullptr;
};
