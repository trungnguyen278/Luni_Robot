#pragma once

#include <string>
#include <functional>
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/event_groups.h"
#include "cJSON.h"

class WebSocketClient {
public:
    WebSocketClient();
    ~WebSocketClient();

    void init();

    // Connect with MAC-based identification
    esp_err_t connect(const char* url);
    void disconnect();
    bool isConnected() const { return connected_; }
    bool isAuthenticated() const { return authenticated_; }

    // Text frames (JSON commands/status)
    esp_err_t sendText(const char* json, size_t len);
    esp_err_t sendMessage(cJSON* msg);  // Auto-serialize + delete

    // Binary frames (audio)
    esp_err_t sendBinary(const uint8_t* data, size_t len);
    esp_err_t sendAudioFrame(uint16_t seq, const uint8_t* opus_data, size_t opus_len);

    // TX buffer management
    void waitTxDrain(uint32_t timeout_ms = 500);
    size_t getTxFreeSpace() const;
    void resetTxBuffer();

    // Callbacks
    using TextCallback   = std::function<void(const char* data, size_t len)>;
    using BinaryCallback = std::function<void(const uint8_t* data, size_t len)>;
    using StateCallback  = std::function<void(bool connected)>;
    using AuthCallback   = std::function<void(bool success, int close_code)>;

    void onText(TextCallback cb)       { text_cb_ = std::move(cb); }
    void onBinary(BinaryCallback cb)   { binary_cb_ = std::move(cb); }
    void onStateChange(StateCallback cb) { state_cb_ = std::move(cb); }
    void onAuthResult(AuthCallback cb) { auth_cb_ = std::move(cb); }

    // Device info for auth
    void setDeviceInfo(const char* mac, const char* fw_version, const char* model);

private:
    static void eventHandlerStatic(void* handler_args, esp_event_base_t base,
                                   int32_t event_id, void* event_data);
    void eventHandler(esp_event_base_t base, int32_t event_id,
                      esp_websocket_event_data_t* data);

    static void txTaskEntry(void* arg);
    void txLoop();

    esp_err_t authenticate();

    esp_websocket_client_handle_t client_ = nullptr;
    std::string url_;
    std::string mac_;
    std::string fw_version_;
    std::string model_;

    bool connected_ = false;
    bool authenticated_ = false;
    int last_close_code_ = 0;

    TextCallback   text_cb_;
    BinaryCallback binary_cb_;
    StateCallback  state_cb_;
    AuthCallback   auth_cb_;

    StreamBufferHandle_t tx_buffer_ = nullptr;
    TaskHandle_t tx_task_ = nullptr;
    bool run_tx_task_ = false;

    EventGroupHandle_t auth_event_ = nullptr;
    static constexpr int AUTH_SUCCESS_BIT = (1 << 0);
    static constexpr int AUTH_FAIL_BIT    = (1 << 1);
};
