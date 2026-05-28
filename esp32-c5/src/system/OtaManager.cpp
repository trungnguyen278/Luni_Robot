#include "OtaManager.hpp"
#include "system/StateManager.hpp"
#include "HttpClient.hpp"
#include "config/ServerConfig.hpp"

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "mbedtls/sha256.h"
#include <cstring>
#include <cstdio>

static const char* TAG = "OtaManager";

void OtaManager::init(StateManager& sm)
{
    sm_ = &sm;
}

void OtaManager::checkUpdate(const char* current_version, const char* model)
{
    if (!sm_) return;
    if (state_ != state::OtaState::IDLE) return;

    sm_->setOtaState(state::OtaState::CHECKING);
    state_ = state::OtaState::CHECKING;

    ESP_LOGI(TAG, "Check OTA: version=%s model=%s", current_version, model);

    // OTA check is triggered by server pushing OTA_AVAILABLE via WS
    // No HTTP polling needed — server knows our version from device_info
    state_ = state::OtaState::IDLE;
    sm_->setOtaState(state::OtaState::IDLE);
}

void OtaManager::startDownload(const char* url, const char* sha256, size_t size)
{
    if (!sm_ || !url || !sha256) return;
    if (state_ != state::OtaState::IDLE && state_ != state::OtaState::CHECKING) return;

    strncpy(pending_.url, url, sizeof(pending_.url) - 1);
    strncpy(pending_.sha256, sha256, sizeof(pending_.sha256) - 1);
    pending_.size = size;
    cancel_requested_ = false;

    sm_->setOtaState(state::OtaState::DOWNLOADING);
    state_ = state::OtaState::DOWNLOADING;
    progress_ = 0;

    xTaskCreatePinnedToCore(&OtaManager::downloadTaskEntry, "ota_dl",
                            8192, this, 3, &dl_task_, 0);
}

void OtaManager::cancel()
{
    cancel_requested_ = true;
}

void OtaManager::downloadTaskEntry(void* arg)
{
    static_cast<OtaManager*>(arg)->downloadTask();
}

void OtaManager::downloadTask()
{
    ESP_LOGI(TAG, "OTA download: %s (%zu bytes)", pending_.url, pending_.size);

    const esp_partition_t* update_part = esp_ota_get_next_update_partition(nullptr);
    if (!update_part) {
        ESP_LOGE(TAG, "No OTA partition found");
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        vTaskDelete(nullptr);
        return;
    }

    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_part, pending_.size, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        vTaskDelete(nullptr);
        return;
    }

    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);

    HttpClient http;
    size_t total_written = 0;

    err = http.download(pending_.url, pending_.size,
        [&](const uint8_t* data, size_t len) {
            esp_ota_write(ota_handle, data, len);
            mbedtls_sha256_update(&sha_ctx, data, len);
            total_written += len;
        },
        [&](size_t downloaded, size_t total) {
            uint8_t pct = (total > 0) ? (uint8_t)(downloaded * 100 / total) : 0;
            progress_ = pct;
        });

    if (cancel_requested_ || err != ESP_OK) {
        ESP_LOGW(TAG, "OTA download %s", cancel_requested_ ? "cancelled" : "failed");
        esp_ota_abort(ota_handle);
        mbedtls_sha256_free(&sha_ctx);
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        dl_task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    // Verify SHA256
    state_ = state::OtaState::VERIFYING;
    sm_->setOtaState(state::OtaState::VERIFYING);

    uint8_t hash[32];
    mbedtls_sha256_finish(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);

    char hash_str[65];
    for (int i = 0; i < 32; ++i) {
        sprintf(&hash_str[i * 2], "%02x", hash[i]);
    }
    hash_str[64] = '\0';

    if (strncmp(hash_str, pending_.sha256, 64) != 0) {
        ESP_LOGE(TAG, "SHA256 mismatch: expected=%s got=%s", pending_.sha256, hash_str);
        esp_ota_abort(ota_handle);
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        dl_task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    // Finalize
    state_ = state::OtaState::FLASHING;
    sm_->setOtaState(state::OtaState::FLASHING);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        dl_task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    err = esp_ota_set_boot_partition(update_part);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
        state_ = state::OtaState::FAILED;
        sm_->setOtaState(state::OtaState::FAILED);
        dl_task_ = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "OTA complete, rebooting in 3s");
    state_ = state::OtaState::REBOOTING;
    sm_->setOtaState(state::OtaState::REBOOTING);
    progress_ = 100;

    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}
