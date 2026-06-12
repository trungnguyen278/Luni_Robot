#pragma once
#include <functional>
#include <atomic>
#include <cstdint>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "UartProtocol.hpp"

// UartBridge for ESP32-C5: sends STATUS_UPDATE to S3, receives CONTROL_CMD from S3.
// Uses UART1 at 460800 baud. Lightweight alternative to SPI for control messages.
class UartBridge {
public:
    struct Config {
        gpio_num_t pin_tx;
        gpio_num_t pin_rx;
        uart_port_t uart_num = UART_NUM_1;
        int baud_rate = 115200;
    };

    using ControlCmdCb = std::function<void(uart_proto::ControlCmd cmd,
                                             const uint8_t* data, size_t len)>;
    using LogEntryCb = std::function<void(const uint8_t* data, size_t len)>;
    // Fired when a full camera JPEG has been reassembled from IMAGE_CHUNK frames.
    using ImageCb = std::function<void(const uint8_t* jpeg, size_t len)>;
    // Fired per IMAGE_CHUNK frame so the consumer can stream it on (e.g. to the
    // server) without the C5 ever buffering the whole image. `payload` is the raw
    // UART IMAGE_CHUNK payload: [seq:2 LE][flags:1][data...].
    using ImageChunkCb = std::function<void(const uint8_t* payload, uint8_t len)>;

    UartBridge() = default;
    ~UartBridge();

    bool init(const Config& cfg);
    void start();
    void stop();

    // Send status update to S3 (non-blocking)
    bool sendStatusUpdate(uint8_t interaction, uint8_t connectivity,
                          uint8_t system_state, uint8_t emotion);

    // Send new message types to S3
    bool sendSyncData(const uint8_t* data, size_t len);
    bool sendOtaStatus(uint8_t state, uint8_t progress);
    bool sendDeviceCmd(uart_proto::ControlCmd cmd,
                       const uint8_t* data = nullptr, size_t len = 0);

    // Callbacks for messages received from S3
    void onControlCmd(ControlCmdCb cb) { control_cb_ = std::move(cb); }
    void onLogEntry(LogEntryCb cb) { log_cb_ = std::move(cb); }
    void onImageComplete(ImageCb cb) { image_cb_ = std::move(cb); }
    // When set, IMAGE_CHUNK frames are streamed out per-chunk via this callback
    // and NOT reassembled on the C5 (takes precedence over onImageComplete).
    void onImageChunk(ImageChunkCb cb) { image_chunk_cb_ = std::move(cb); }

private:
    bool sendFrame(uart_proto::MsgType type, const uint8_t* payload, uint8_t len);

    static void rxTaskEntry(void* arg);
    void rxLoop();

    // Camera image reassembly (IMAGE_CHUNK frames from S3)
    void handleImageChunk(const uint8_t* payload, uint8_t len);
    void resetImage();

    Config cfg_{};
    std::atomic<bool> started_{false};
    TaskHandle_t rx_task_ = nullptr;
    ControlCmdCb control_cb_;
    LogEntryCb log_cb_;
    ImageCb     image_cb_;
    ImageChunkCb image_chunk_cb_;

    uint8_t* img_buf_   = nullptr;
    size_t   img_total_ = 0;
    size_t   img_recv_  = 0;
    uint16_t img_seq_   = 0;
};
