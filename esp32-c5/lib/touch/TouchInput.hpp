#pragma once
#include <cstdint>
#include <functional>
#include <atomic>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

/**
 * Class:   TouchInput
 * Author:  Trung Nguyen
 *
 * Description:
 *  - Đọc trạng thái cảm ứng / nút bấm từ một chân GPIO
 *  - Hỗ trợ debounce, phát hiện nhấn dài
 *  - Gọi callback khi có sự kiện
 *  - Chạy trong task riêng để không block main thread
 *  - Cấu hình đơn giản qua struct Config
 *
 *  Copied from the ESP32-S3 firmware so the C5 can own the push-to-talk button
 *  (moved here from S3 to free pins on the Freenove ESP32-S3-WROOM CAM board).
 */
class TouchInput
{
public:
    enum class Event : uint8_t
    {
        PRESS,
        RELEASE,
        LONG_PRESS
    };

    struct Config
    {
        gpio_num_t pin;
        bool active_low = true;
        uint32_t long_press_ms = 1500;
        uint32_t debounce_ms = 30;
    };

public:
    TouchInput() = default;
    ~TouchInput();

    //======= Lifecycle =======
    bool init(const Config &cfg);
    void start();
    void stop();

    //======= Event callback =======
    void onEvent(std::function<void(Event)> cb);

private:
    static void taskEntry(void *arg);
    void loop();

    bool readRaw() const;

private:
    Config cfg_{};
    std::function<void(Event)> cb_;

    TaskHandle_t task_ = nullptr;
    std::atomic<bool> running{false};

    bool last_state = false;
    TickType_t press_tick = 0;
};
