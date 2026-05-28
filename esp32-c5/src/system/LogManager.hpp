#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class WebSocketClient;

class LogManager {
public:
    static constexpr uint8_t LOG_DEBUG = 10;
    static constexpr uint8_t LOG_INFO  = 20;
    static constexpr uint8_t LOG_WARN  = 30;
    static constexpr uint8_t LOG_ERROR = 40;

    static LogManager& instance();

    void init(uint8_t defaultLevel = LOG_INFO);
    void setLevel(uint8_t level) { min_level_ = level; }
    uint8_t getLevel() const { return min_level_; }

    void debug(const char* tag, const char* fmt, ...);
    void info(const char* tag, const char* fmt, ...);
    void warn(const char* tag, const char* fmt, ...);
    void error(const char* tag, const char* fmt, ...);

    // Flush pending logs via WebSocket (batched)
    void flush(WebSocketClient& ws);

    size_t pendingCount() const { return count_; }
    bool shouldFlush() const { return count_ > 0 && (count_ >= BUFFER_SIZE / 2 || flush_pending_); }
    void requestFlush() { flush_pending_ = true; }

private:
    LogManager() = default;

    void log(uint8_t level, const char* tag, const char* fmt, va_list args);
    static const char* levelToString(uint8_t level);

    static constexpr size_t BUFFER_SIZE = 64;
    static constexpr size_t MSG_MAX_LEN = 128;
    static constexpr size_t TAG_MAX_LEN = 16;

    struct LogEntry {
        uint8_t level;
        char tag[TAG_MAX_LEN];
        char message[MSG_MAX_LEN];
        uint32_t timestamp;
    };

    LogEntry buffer_[BUFFER_SIZE] = {};
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
    SemaphoreHandle_t mutex_ = nullptr;

    std::atomic<uint8_t> min_level_{LOG_INFO};
    bool flush_pending_ = false;
    bool initialized_ = false;
};

#define LLOG_D(tag, fmt, ...) LogManager::instance().debug(tag, fmt, ##__VA_ARGS__)
#define LLOG_I(tag, fmt, ...) LogManager::instance().info(tag, fmt, ##__VA_ARGS__)
#define LLOG_W(tag, fmt, ...) LogManager::instance().warn(tag, fmt, ##__VA_ARGS__)
#define LLOG_E(tag, fmt, ...) LogManager::instance().error(tag, fmt, ##__VA_ARGS__)
