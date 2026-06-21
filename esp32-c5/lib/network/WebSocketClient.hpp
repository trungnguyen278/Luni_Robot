#pragma once

#include <string>
#include <functional>
#include "esp_websocket_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/stream_buffer.h"
#include "freertos/queue.h"
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
    // True only when the server explicitly rejected our auth (sent an
    // auth_result with status != "ok") — i.e. a bad/revoked device_token, not a
    // transport drop. Lets NetworkManager pick AUTH_REJECTED (→ BLE fallback)
    // instead of treating a permanent rejection as a retryable handshake fail.
    bool wasAuthRejected() const { return auth_rejected_; }

    // Text frames (JSON commands/status). Enqueued onto a single TX queue and
    // sent from a dedicated task — the caller (esp_timer / WS-event / net_conn
    // task) never blocks inside the websocket send. Returns ESP_OK once queued.
    esp_err_t sendText(const char* json, size_t len);
    esp_err_t sendMessage(cJSON* msg);  // Auto-serialize + delete

    // Binary frames (audio)
    esp_err_t sendBinary(const uint8_t* data, size_t len);
    esp_err_t sendAudioFrame(uint16_t seq, const uint8_t* opus_data, size_t opus_len);

    // Send a complete binary payload as exactly ONE WebSocket frame, bypassing
    // the audio TX batch buffer. Required for camera images: the server expects
    // the whole 0xAC+JPEG in a single frame, but the byte-stream batching used
    // for audio would split it across 2048-byte frames and corrupt it.
    esp_err_t sendBinaryWhole(const uint8_t* data, size_t len);

    // TX buffer management
    void waitTxDrain(uint32_t timeout_ms = 500);
    size_t getTxFreeSpace() const;
    void resetTxBuffer();

    // Callbacks
    using TextCallback   = std::function<void(const char* data, size_t len)>;
    // Binary messages larger than the WS rx buffer arrive as multiple
    // fragments: `offset` is this fragment's position, `total` the full
    // message length. offset==0 marks the start of a new message;
    // offset+len==total marks the end.
    using BinaryCallback = std::function<void(const uint8_t* data, size_t len,
                                              size_t offset, size_t total)>;
    using StateCallback  = std::function<void(bool connected)>;
    using AuthCallback   = std::function<void(bool success, int close_code)>;

    void onText(TextCallback cb)       { text_cb_ = std::move(cb); }
    void onBinary(BinaryCallback cb)   { binary_cb_ = std::move(cb); }
    void onStateChange(StateCallback cb) { state_cb_ = std::move(cb); }
    void onAuthResult(AuthCallback cb) { auth_cb_ = std::move(cb); }

    // Device info for auth
    void setDeviceInfo(const char* mac, const char* device_token,
                       const char* fw_version, const char* model);

private:
    static void eventHandlerStatic(void* handler_args, esp_event_base_t base,
                                   int32_t event_id, void* event_data);
    void eventHandler(esp_event_base_t base, int32_t event_id,
                      esp_websocket_event_data_t* data);

    static void txTaskEntry(void* arg);
    void txLoop();

    // Single text-frame TX queue: senders enqueue a heap copy (non-blocking),
    // the ws_txt task drains it and does the (blocking) websocket send.
    static void textTaskEntry(void* arg);
    void textLoop();
    esp_err_t enqueueText(const char* json, size_t len);

    esp_err_t sendAuthMessage();

    esp_websocket_client_handle_t client_ = nullptr;
    std::string url_;
    std::string mac_;
    std::string device_token_;
    std::string fw_version_;
    std::string model_;

    bool connected_ = false;
    bool authenticated_ = false;
    bool auth_rejected_ = false;
    int last_close_code_ = 0;
    // Opcode of the message currently being delivered in fragments —
    // continuation events (op_code 0x0) inherit it.
    uint8_t last_data_opcode_ = 0;

    TextCallback   text_cb_;
    BinaryCallback binary_cb_;
    StateCallback  state_cb_;
    AuthCallback   auth_cb_;

    StreamBufferHandle_t tx_buffer_ = nullptr;
    TaskHandle_t tx_task_ = nullptr;
    bool run_tx_task_ = false;

    // Text TX queue holds owning heap copies of serialized JSON; the ws_txt
    // task frees each after sending. Kept separate from the binary audio
    // tx_buffer_ (which tags every frame 0xAA) so text framing stays correct.
    struct TxTextItem { char* data; uint16_t len; };
    QueueHandle_t text_queue_ = nullptr;
    TaskHandle_t  text_task_  = nullptr;
    bool run_text_task_ = false;

    EventGroupHandle_t auth_event_ = nullptr;
    static constexpr int AUTH_SUCCESS_BIT = (1 << 0);
    static constexpr int AUTH_FAIL_BIT    = (1 << 1);
};
