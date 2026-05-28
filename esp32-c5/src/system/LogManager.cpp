#include "LogManager.hpp"
#include "WebSocketClient.hpp"
#include "protocol/WsProtocol.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include <cstdio>
#include <cstring>

static const char* TAG = "LogManager";

LogManager& LogManager::instance()
{
    static LogManager inst;
    return inst;
}

void LogManager::init(uint8_t defaultLevel)
{
    if (initialized_) return;
    min_level_ = defaultLevel;
    mutex_ = xSemaphoreCreateMutex();
    initialized_ = true;
    ESP_LOGI(TAG, "LogManager init (level=%d)", defaultLevel);
}

void LogManager::log(uint8_t level, const char* tag, const char* fmt, va_list args)
{
    if (level < min_level_ || !initialized_ || !mutex_) return;

    LogEntry entry{};
    entry.level = level;
    entry.timestamp = (uint32_t)(esp_timer_get_time() / 1000);
    strncpy(entry.tag, tag ? tag : "?", TAG_MAX_LEN - 1);
    vsnprintf(entry.message, MSG_MAX_LEN, fmt, args);

    // Also log to ESP console
    switch (level) {
    case LOG_DEBUG: ESP_LOGD(tag, "%s", entry.message); break;
    case LOG_INFO:  ESP_LOGI(tag, "%s", entry.message); break;
    case LOG_WARN:  ESP_LOGW(tag, "%s", entry.message); break;
    case LOG_ERROR: ESP_LOGE(tag, "%s", entry.message); break;
    }

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) == pdTRUE) {
        buffer_[head_] = entry;
        head_ = (head_ + 1) % BUFFER_SIZE;
        if (count_ < BUFFER_SIZE) {
            count_++;
        } else {
            tail_ = (tail_ + 1) % BUFFER_SIZE;
        }
        xSemaphoreGive(mutex_);
    }
}

void LogManager::debug(const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LOG_DEBUG, tag, fmt, args);
    va_end(args);
}

void LogManager::info(const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LOG_INFO, tag, fmt, args);
    va_end(args);
}

void LogManager::warn(const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LOG_WARN, tag, fmt, args);
    va_end(args);
}

void LogManager::error(const char* tag, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log(LOG_ERROR, tag, fmt, args);
    va_end(args);
}

const char* LogManager::levelToString(uint8_t level)
{
    switch (level) {
    case LOG_DEBUG: return "debug";
    case LOG_INFO:  return "info";
    case LOG_WARN:  return "warn";
    case LOG_ERROR: return "error";
    default:        return "info";
    }
}

void LogManager::flush(WebSocketClient& ws)
{
    if (!initialized_ || !mutex_ || count_ == 0) return;
    if (!ws.isConnected() || !ws.isAuthenticated()) return;

    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(50)) != pdTRUE) return;

    size_t to_send = count_;
    if (to_send > 10) to_send = 10;

    for (size_t i = 0; i < to_send; ++i) {
        LogEntry& entry = buffer_[tail_];

        cJSON* msg = ws::createLog(
            levelToString(entry.level),
            entry.tag,
            entry.message);

        if (msg) {
            char* json = cJSON_PrintUnformatted(msg);
            cJSON_Delete(msg);
            if (json) {
                ws.sendText(json, strlen(json));
                cJSON_free(json);
            }
        }

        tail_ = (tail_ + 1) % BUFFER_SIZE;
        count_--;
    }

    flush_pending_ = false;
    xSemaphoreGive(mutex_);
}
