#include "AppController.hpp"
#include "config/DLuniceProfile.hpp"
#include "system/StateManager.hpp"
#include "Version.hpp"
#include "esp_log.h"

static const char* TAG = "main";

extern "C" void app_main()
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Luni V2 - ESP32-S3");
    ESP_LOGI(TAG, "  Version: %s", app_meta::APP_VERSION);
    ESP_LOGI(TAG, "  Model:   %s", app_meta::DLuniCE_MODEL);
    ESP_LOGI(TAG, "  Build:   %s", app_meta::BUILD_DATE);
    ESP_LOGI(TAG, "  ID:      %s", getDLuniceEfuseID());
    ESP_LOGI(TAG, "========================================");

    auto& app = AppController::instance();

    // DLuniceProfile creates and wires all modules
    if (!DLuniceProfile::setup(app)) {
        ESP_LOGE(TAG, "DLuniceProfile setup FAILED");
        StateManager::instance().setSystemState(state::SystemState::ERROR);
        return;
    }

    // Initialize AppController (sets up event queue + subscriptions)
    if (!app.init()) {
        ESP_LOGE(TAG, "AppController init FAILED");
        StateManager::instance().setSystemState(state::SystemState::ERROR);
        return;
    }

    // Start all modules
    app.start();

    // Mark system as running
    StateManager::instance().setSystemState(state::SystemState::RUNNING);

    ESP_LOGI(TAG, "ESP32-S3 startup complete");
}
