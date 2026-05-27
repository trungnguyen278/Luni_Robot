#include "DisplayManager.hpp"
#include "DisplayDriver.hpp"

#include <utility>
#include <atomic>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

static const char *TAG = "DisplayManager";

DisplayManager::DisplayManager() = default;
DisplayManager::~DisplayManager()
{
    // Ensure task is stopped before destruction
    stopLoop();
    // Unsubscribe
    if (binding_enabled)
    {
        auto &sm = StateManager::instance();
        if (sub_inter != -1)
            sm.unsubscribeInteraction(sub_inter);
        if (sub_conn != -1)
            sm.unsubscribeConnectivity(sub_conn);
        if (sub_sys != -1)
            sm.unsubscribeSystem(sub_sys);
        if (sub_power != -1)
            sm.unsubscribePower(sub_power);
        if (sub_emotion != -1)
            sm.unsubscribeEmotion(sub_emotion);
    }
}

// ----------------------------------------------------------------------------
// Init
// ----------------------------------------------------------------------------
bool DisplayManager::init(std::unique_ptr<DisplayDriver> driver, int width, int height)
{
    if (!driver)
    {
        ESP_LOGE(TAG, "init: DisplayDriver is null");
        return false;
    }

    drv = std::move(driver);
    width_ = width;
    height_ = height;

    drv->setRotation(3); // Landscape (rotated 180°)
    // Sync dimensions after rotation (driver swaps width↔height for landscape)
    width_ = drv->width();
    height_ = drv->height();

    // GfxEngine: PSRAM-backed framebuffer for procedural RGB565 rendering
    gfx_ = std::make_unique<GfxEngine>(width_, height_);
    if (!gfx_->init()) {
        ESP_LOGE(TAG, "GfxEngine init failed (PSRAM?)");
        return false;
    }
    scene_mgr_.showEmotion("normal", "normal-steady");
    ESP_LOGI(TAG, "GfxEngine initialized — procedural rendering active");

    ESP_LOGI(TAG, "DisplayManager init OK (%dx%d)", width_, height_);
    return true;
}

// ----------------------------------------------------------------------------
// Task loop management
// ----------------------------------------------------------------------------
bool DisplayManager::startLoop(uint32_t interval_ms,
                               UBaseType_t priority,
                               uint32_t stackSize,
                               BaseType_t core)
{
    if (task_handle_ != nullptr)
    {
        ESP_LOGW(TAG, "startLoop: already running");
        update_interval_ms_ = interval_ms;
        return true;
    }
    if (!drv || !gfx_)
    {
        ESP_LOGE(TAG, "startLoop: not initialized");
        return false;
    }

    update_interval_ms_ = interval_ms;

#if defined(ESP_PLATFORM)
    BaseType_t rc = xTaskCreatePinnedToCore(
        &DisplayManager::taskEntry,
        "DisplayLoop",
        stackSize,
        this,
        priority,
        &task_handle_,
        core);
#else
    BaseType_t rc = xTaskCreate(
        &DisplayManager::taskEntry,
        "DisplayLoop",
        stackSize,
        this,
        priority,
        &task_handle_);
#endif

    if (rc != pdPASS)
    {
        ESP_LOGE(TAG, "startLoop: xTaskCreate failed (%d)", (int)rc);
        task_handle_ = nullptr;
        return false;
    }
    ESP_LOGI(TAG, "Display loop started (interval=%ums)", (unsigned)update_interval_ms_);
    return true;
}

void DisplayManager::stopLoop()
{
    if (task_handle_ == nullptr)
        return;

    // ✅ Signal graceful shutdown instead of force-deleting
    task_running_.store(false);

    // Wait for task to exit (max 1 second)
    uint32_t wait_ms = 0;
    const uint32_t TIMEOUT_MS = 1000;
    while (task_handle_ != nullptr && wait_ms < TIMEOUT_MS)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
        wait_ms += 10;
    }

    if (task_handle_ != nullptr)
    {
        ESP_LOGW(TAG, "Display task did not exit; force deleting");
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }

    ESP_LOGI(TAG, "Display loop stopped");
}

// ----------------------------------------------------------------------------
// State binding
// ----------------------------------------------------------------------------
void DisplayManager::enableStateBinding(bool enable)
{
    binding_enabled = enable;

    if (!enable)
        return;

    auto &sm = StateManager::instance();

    sub_inter = sm.subscribeInteraction([this](state::InteractionState s, state::InputSource src)
                                        { this->handleInteraction(s, src); });

    sub_conn = sm.subscribeConnectivity([this](state::ConnectionState s)
                                        { this->handleConnectivity(s); });

    sub_sys = sm.subscribeSystem([this](state::SystemState s)
                                 { this->handleSystem(s); });

    sub_power = sm.subscribePower([this](state::PowerState s)
                                  { this->handlePower(s); });

    sub_emotion = sm.subscribeEmotion([this](state::EmotionState s)
                                      { this->handleEmotion(s); });

    ESP_LOGI(TAG, "DisplayManager state binding enabled");

    // Trigger initial state display — default states (BOOTING, OFFLINE) never
    // fire callbacks because setXxxState() guards against same-value writes.
    handleSystem(sm.getSystemState());
}

// (public show* helpers removed; UI is driven via handlers)

void DisplayManager::setBatteryPercent(uint8_t p)
{
    battery_percent = p;
}

// (toast feature removed)

// ----------------------------------------------------------------------------
// Power saving mode
// ----------------------------------------------------------------------------
void DisplayManager::setPowerSaveMode(bool enable)
{
    if (enable) {
        setBacklight(false);
    } else {
        setBacklight(true);
    }
}

void DisplayManager::setBacklight(bool on)
{
    if (drv)
    {
        drv->setBacklight(on);
    }
}

void DisplayManager::setBrightness(uint8_t percent)
{
    if (drv)
    {
        drv->setBacklightLevel(percent);
    }
}

// ----------------------------------------------------------------------------
// Asset registration
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Update (should be called from UI task)
void DisplayManager::update(uint32_t dt_ms)
{
    if (!drv)
        return;

    // Sync dimensions from driver (in case rotation changed)
    width_ = drv->width();
    height_ = drv->height();

    // ✅ DEBUG: Log first update to verify task is running
    static bool first_update = true;
    if (first_update)
    {
        ESP_LOGI(TAG, "First update() called - display loop is working, dimensions=%dx%d", width_, height_);
        first_update = false;
    }

    // 0) If text mode active, render text only (no animation)
    if (text_active_)
    {
        // Only clear once when entering text mode
        if (!text_mode_cleared_)
        {
            drv->fillScreen(0x0000, 0, 22, width_, height_ - 22); // Clear below top bar
            text_mode_cleared_ = true;
        }
        if (!text_msg_.empty())
        {
            drv->drawTextCenter(text_msg_.c_str(), text_color_, width_ / 2, height_ / 2, text_scale_); // Center of screen
        }
        return;
    }
    else
    {
        // Reset flag when not in text mode
        text_mode_cleared_ = false;
    }

    // Procedural rendering via GfxEngine
    gfx_->clear(COLOR_BG);

    // Tick idle loop (picks new variant every 3-6s when idle)
    scene_mgr_.tickIdle((float)dt_ms);

    // Render current variant
    scene_mgr_.update(*gfx_, (float)dt_ms);

    // Status bar (top 20px, always cyan)
    gfx_->fillRect(0, 0, width_, geom::STATUS_H, COLOR_BG);
    gfx_->drawLine(0, geom::STATUS_H, width_, geom::STATUS_H,
                   TONE_LUT[TONE_CYAN], 1, 64); // 25% opacity separator

    auto& sd = scene_mgr_.getSceneData();
    if (sd.time_valid) {
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", sd.hours, sd.minutes);
        gfx_->drawText(timeBuf, 8, 4, TONE_LUT[TONE_CYAN], 1,
                       GfxEngine::TextAlign::LEFT, 217);
    }

    // Battery overlay
    if (battery_percent < 255) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", battery_percent);
        gfx_->drawText(buf, width_ - 40, 4, TONE_LUT[TONE_CYAN], 1,
                       GfxEngine::TextAlign::LEFT, 217);
    }

    // Flush to display
    gfx_->flush(drv.get());
}

void DisplayManager::taskEntry(void *arg)
{
    auto *self = static_cast<DisplayManager *>(arg);
    self->task_running_.store(true); // ✅ Signal task is running
    TickType_t prev = xTaskGetTickCount();

    while (self->task_running_.load())
    { // ✅ Check graceful shutdown flag
        TickType_t now = xTaskGetTickCount();
        uint32_t dt_ms = (now - prev) * portTICK_PERIOD_MS;
        prev = now;
        ESP_LOGD(TAG, "DisplayManager update dt_ms=%u", dt_ms);
        self->update(dt_ms);
        vTaskDelay(pdMS_TO_TICKS(self->update_interval_ms_));
    }

    // ✅ Graceful exit: cleanup and notify stopper
    self->task_handle_ = nullptr;
    vTaskDelete(nullptr);
}

// ----------------------------------------------------------------------------
// MAPPING STATE → UI BEHAVIOR
// ----------------------------------------------------------------------------

void DisplayManager::handleInteraction(state::InteractionState s, state::InputSource src)
{
    switch (s)
    {
    case state::InteractionState::TRIGGERED:
    case state::InteractionState::LISTENING:
        playEmotion("listening");
        break;
    case state::InteractionState::PROCESSING:
        playEmotion("thinking");
        break;
    case state::InteractionState::SPEAKING:
        // playEmotion("speaking");
        break;
    case state::InteractionState::CANCELLING:
        break;
    case state::InteractionState::MUTED:
        break;
    case state::InteractionState::SLEEPING:
        setPowerSaveMode(true);
        break;
    case state::InteractionState::IDLE:
    default:
        playEmotion("idle");
        break;
    }
}

void DisplayManager::handleConnectivity(state::ConnectionState s)
{
    switch (s)
    {
    case state::ConnectionState::OFFLINE:
        playText("Offline", -1, -1, 0xFFFF, 1.5);
        break;

    case state::ConnectionState::WIFI_CONNECTING:
        playText("Connecting WiFi...", -1, -1, 0xFFFF, 1.8);
        break;

    case state::ConnectionState::WIFI_CONNECTED:
        playText("WiFi Connected", -1, -1, 0xFFFF, 1.8);
        break;

    case state::ConnectionState::WS_CONNECTING:
    case state::ConnectionState::WS_AUTHENTICATING:
        playEmotion("stun");
        break;

    case state::ConnectionState::ONLINE:
        playEmotion("idle");
        break;

    case state::ConnectionState::RECONNECTING:
        playText("Reconnecting...", -1, -1, 0xFFFF, 1.5);
        break;

    case state::ConnectionState::BLE_PROVISIONING:
        playText("BLE Setup", -1, -1, 0xFFFF, 1.8);
        break;
    }
}

void DisplayManager::handleSystem(state::SystemState s)
{
    switch (s)
    {
    case state::SystemState::BOOTING:
        // ESP_LOGI(TAG, "Displaying Booting message");
        playText("Luni", 40, 0, 0xF800, 2); // red
        break;

    case state::SystemState::RUNNING:
        handleConnectivity(StateManager::instance().getConnectionState());
        break;

    case state::SystemState::ERROR:
        playEmotion("error");
        break;

    case state::SystemState::MAINTENANCE:
        playEmotion("maintenance");
        break;
    }
}

void DisplayManager::handlePower(state::PowerState s)
{
    switch (s)
    {
    case state::PowerState::NORMAL:
        break;

    case state::PowerState::CHARGING:
        scene_mgr_.showScene("power", "power-charging");
        break;

    case state::PowerState::FULL:
        scene_mgr_.showScene("power", "power-full");
        break;

    case state::PowerState::CRITICAL:
        ESP_LOGI(TAG, "CRITICAL: low battery");
        scene_mgr_.showScene("power", "power-critical");
        break;

    default:
        break;
    }
}

void DisplayManager::handleEmotion(state::EmotionState s)
{
    // Map emotion state to an available emotion asset
    switch (s)
    {
    case state::EmotionState::HAPPY:
        playEmotion("happy");
        break;
    case state::EmotionState::SAD:
        playEmotion("sad");
        break;
    case state::EmotionState::THINKING:
        playEmotion("thinking");
        break;
    case state::EmotionState::CONFUSED:
        playEmotion("stun");
        break;
    case state::EmotionState::NEUTRAL:
        playEmotion("neutral");
        break;
    case state::EmotionState::CALM:
    case state::EmotionState::EXCITED:
    case state::EmotionState::ANGRY:
    default:
        playEmotion("idle");
        break;
    }
}

void DisplayManager::handleSyncData(const state::SyncDataPacket& data)
{
    SceneData& sd = scene_mgr_.getSceneData();
    sd.temperature = data.temperature;
    sd.humidity = data.humidity;
    sd.weather_condition = data.condition;
    sd.aqi = data.aqi;
    sd.unix_time = data.unix_time;
    sd.utc_offset = data.utc_offset;
    sd.lunar_day = data.lunar_day;
    sd.lunar_month = data.lunar_month;
    strncpy(sd.city, data.city, sizeof(sd.city) - 1);
    sd.weather_valid = true;
    sd.time_valid = true;
}

void DisplayManager::handleOtaStatus(state::OtaState ota_state, uint8_t progress)
{
    switch (ota_state) {
    case state::OtaState::DOWNLOADING:
        if (!ota_updating) showOTAUpdating();
        setOTAProgress(progress);
        setOTAStatus("Downloading...");
        break;
    case state::OtaState::VERIFYING:
        setOTAStatus("Verifying...");
        break;
    case state::OtaState::FLASHING:
        setOTAProgress(100);
        setOTAStatus("Flashing...");
        break;
    case state::OtaState::REBOOTING:
        showRebooting();
        break;
    case state::OtaState::FAILED:
        showOTAError("Update failed");
        break;
    default:
        break;
    }
}

// ----------------------------------------------------------------------------
// Internal asset playback
void DisplayManager::playEmotion(const std::string &name, int x, int y)
{
    text_active_ = false;
    scene_mgr_.showEmotion(name.c_str());
}

void DisplayManager::playText(const std::string &text, int x, int y, uint16_t color, int scale)
{
    ESP_LOGI(TAG, "playText '%s' at (%d,%d) color=0x%04X scale=%d", text.c_str(), x, y, color, scale);
    if (scale < 1)
        scale = 1; // keep visible text size
    text_msg_ = text;
    text_x_ = x;
    text_y_ = y;
    text_color_ = color;
    text_scale_ = scale;
    text_active_ = true;
    text_mode_cleared_ = false;
}

void DisplayManager::clearText()
{
    text_active_ = false;
    text_mode_cleared_ = false; // Reset clear flag
    text_msg_.clear();
}

// ----------------------------------------------------------------------------
// Procedural rendering control
// ----------------------------------------------------------------------------
void DisplayManager::playProcedural(const char* categoryKey, const char* variantId)
{
    text_active_ = false;
    scene_mgr_.showScene(categoryKey, variantId);
}

// ============================================================================
// OTA Update UI Functions
// ============================================================================

void DisplayManager::showOTAUpdating()
{
    ESP_LOGI(TAG, "Showing OTA updating screen");
    ota_updating = true;
    ota_completed = false;
    ota_error = false;
    ota_progress_percent = 0;
    ota_status_text = "Starting update...";

    if (!drv)
        return;

    drv->fillScreen(0x0000); // OTA screen: full clear needed
    // TODO: Draw "Updating Firmware" title and progress bar outline
    // TODO: Display initial message
}

void DisplayManager::setOTAProgress(uint8_t current_percent)
{
    if (current_percent > 100)
        current_percent = 100;

    ota_progress_percent = current_percent;

    if (!drv)
        return;

    // Update progress bar on display (0-100%)
    // TODO: Draw progress bar visual
    // Example: draw bar from left to right
    // int bar_width = (width_ * current_percent) / 100;
    // drv->drawBitmap(...);  // Draw progress bar scanline

    ESP_LOGD(TAG, "OTA progress: %u%%", current_percent);
}

void DisplayManager::setOTAStatus(const std::string &status)
{
    ota_status_text = status;
    ESP_LOGI(TAG, "OTA status: %s", status.c_str());

    if (!drv)
        return;

    // TODO: Display status text on screen
    // Example: show at bottom of screen
}

void DisplayManager::showOTACompleted()
{
    ESP_LOGI(TAG, "Showing OTA completed screen");
    ota_updating = false;
    ota_completed = true;
    ota_error = false;
    ota_progress_percent = 100;
    ota_status_text = "Update completed!";

    if (!drv)
        return;

    drv->fillScreen(0x0000);
    // TODO: Draw checkmark or success animation
    // TODO: Display "Update Successful" message
}

void DisplayManager::showOTAError(const std::string &error_msg)
{
    ESP_LOGE(TAG, "Showing OTA error: %s", error_msg.c_str());
    ota_updating = false;
    ota_completed = false;
    ota_error = true;
    ota_error_msg = error_msg;
    ota_status_text = "Update failed!";

    if (!drv)
        return;

    drv->fillScreen(0x0000);
    // TODO: Draw error icon (X mark)
    // TODO: Display error message
}

void DisplayManager::showRebooting()
{
    ESP_LOGI(TAG, "Showing rebooting screen");
    ota_updating = false;
    ota_completed = true;
    ota_error = false;
    ota_status_text = "Rebooting...";

    if (!drv)
        return;

    drv->fillScreen(0x0000);
    // TODO: Draw rebooting animation or countdown
    // TODO: Display "DLunice restarting..." message
}
