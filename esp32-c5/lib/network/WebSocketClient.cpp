#include "WebSocketClient.hpp"
#include "protocol/WsProtocol.hpp"
#include "config/ServerConfig.hpp"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include <cstring>

static const char* TAG = "WebSocketClient";

WebSocketClient::WebSocketClient() = default;

WebSocketClient::~WebSocketClient()
{
    disconnect();
}

void WebSocketClient::init()
{
    if (!tx_buffer_) {
        tx_buffer_ = xStreamBufferCreate(server_cfg::WS_TX_BUFFER_SIZE, 1);
    }
    if (!text_queue_) {
        text_queue_ = xQueueCreate(server_cfg::WS_TEXT_QUEUE_DEPTH, sizeof(TxTextItem));
    }
    if (!auth_event_) {
        auth_event_ = xEventGroupCreate();
    }
}

esp_err_t WebSocketClient::connect(const char* url)
{
    if (!url) return ESP_ERR_INVALID_ARG;

    url_ = url;
    authenticated_ = false;
    auth_rejected_ = false;
    last_close_code_ = 0;

    if (client_) {
        ESP_LOGW(TAG, "Already connected, closing old instance");
        disconnect();
    }

    esp_websocket_client_config_t cfg = {};
    cfg.uri = url_.c_str();
    cfg.buffer_size = server_cfg::WS_BUFFER_SIZE;
    cfg.network_timeout_ms = server_cfg::WS_NETWORK_TIMEOUT_MS;
    cfg.disable_auto_reconnect = true;  // We manage reconnect in NetworkManager
    cfg.ping_interval_sec = server_cfg::WS_PING_INTERVAL_SEC;
    cfg.pingpong_timeout_sec = server_cfg::WS_PINGPONG_TIMEOUT_SEC;
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = server_cfg::TCP_KEEPALIVE_IDLE_SEC;
    cfg.keep_alive_interval = server_cfg::TCP_KEEPALIVE_INTERVAL_SEC;
    cfg.keep_alive_count = server_cfg::TCP_KEEPALIVE_COUNT;
    if (url_.rfind("wss://", 0) == 0) {
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
    }

    ESP_LOGI(TAG, "WS init heap: free=%lu largest=%u txbuf=%u",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
             (unsigned)server_cfg::WS_TX_BUFFER_SIZE);

    client_ = esp_websocket_client_init(&cfg);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init websocket (free=%lu largest=%u)",
                 (unsigned long)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(esp_websocket_register_events(
        client_, WEBSOCKET_EVENT_ANY,
        &WebSocketClient::eventHandlerStatic, this));

    ESP_LOGI(TAG, "Connecting to %s", url_.c_str());
    esp_err_t err = esp_websocket_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WS start failed: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
        return err;
    }

    if (!tx_task_) {
        run_tx_task_ = true;
        xTaskCreate(&WebSocketClient::txTaskEntry, "ws_tx",
                    server_cfg::WS_TX_TASK_STACK_SIZE, this, 6, &tx_task_);
    }

    if (!text_task_) {
        run_text_task_ = true;
        xTaskCreate(&WebSocketClient::textTaskEntry, "ws_txt",
                    server_cfg::WS_TXT_TASK_STACK_SIZE, this, 6, &text_task_);
    }

    return ESP_OK;
}

void WebSocketClient::disconnect()
{
    run_tx_task_ = false;
    run_text_task_ = false;

    if (client_) {
        ESP_LOGI(TAG, "Disconnecting WebSocket");
        esp_websocket_client_close(client_, 100);
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
    }

    connected_ = false;
    authenticated_ = false;

    if (tx_task_) {
        vTaskDelay(pdMS_TO_TICKS(100));
        tx_task_ = nullptr;
    }
    // The ws_txt task self-deletes once run_text_task_ is false; the delay above
    // gives it time to exit before we free leftover items below.
    text_task_ = nullptr;

    if (tx_buffer_) {
        xStreamBufferReset(tx_buffer_);
    }

    // Drop any queued text so a reconnect doesn't replay stale frames (and so
    // their heap copies don't leak). Safe: the drain task has already stopped.
    if (text_queue_) {
        TxTextItem item;
        while (xQueueReceive(text_queue_, &item, 0) == pdTRUE) free(item.data);
    }

    if (state_cb_) state_cb_(false);
}

void WebSocketClient::setDeviceInfo(const char* mac, const char* device_token,
                                    const char* fw_version, const char* model)
{
    if (mac) mac_ = mac;
    if (device_token) device_token_ = device_token;
    if (fw_version) fw_version_ = fw_version;
    if (model) model_ = model;
}

// === Authentication ===

esp_err_t WebSocketClient::sendAuthMessage()
{
    if (!connected_ || !client_) return ESP_FAIL;
    if (auth_event_) {
        xEventGroupClearBits(auth_event_, AUTH_SUCCESS_BIT | AUTH_FAIL_BIT);
    }

    cJSON* auth_msg = ws::createAuthMessage(
        mac_.c_str(), device_token_.c_str(), fw_version_.c_str(), model_.c_str());
    if (!auth_msg) return ESP_ERR_NO_MEM;

    char* json = cJSON_PrintUnformatted(auth_msg);
    cJSON_Delete(auth_msg);
    if (!json) return ESP_ERR_NO_MEM;

    int sent = esp_websocket_client_send_text(client_, json, strlen(json), pdMS_TO_TICKS(1000));
    cJSON_free(json);

    if (sent < 0) {
        ESP_LOGE(TAG, "Failed to send auth message");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Auth message sent, waiting for auth_result...");
    ESP_LOGI(TAG, "Auth credentials: mac=%s token=%s",
             mac_.empty() ? "<empty>" : mac_.c_str(),
             device_token_.empty() ? "<empty>" : "<set>");
    return ESP_OK;
}

// === Send methods ===

esp_err_t WebSocketClient::sendText(const char* json, size_t len)
{
    if (!client_ || !connected_) return ESP_FAIL;
    // Never send inline: that blocks the caller (heartbeat/watchdog/log-flush all
    // run on the shared esp_timer task) for up to 1s. Hand off to the ws_txt task.
    return enqueueText(json, len);
}

esp_err_t WebSocketClient::enqueueText(const char* json, size_t len)
{
    if (!text_queue_ || !json || len == 0) return ESP_FAIL;
    if (len > server_cfg::WS_TEXT_MAX_LEN) {
        ESP_LOGW(TAG, "sendText too large (%zu), dropping", len);
        return ESP_FAIL;
    }

    char* copy = (char*)malloc(len + 1);
    if (!copy) return ESP_ERR_NO_MEM;
    memcpy(copy, json, len);
    copy[len] = '\0';

    TxTextItem item{ copy, (uint16_t)len };
    // Non-blocking: if the queue is full, drop this frame rather than stall the
    // producing task. Losing a heartbeat/state update is recoverable; blocking
    // the esp_timer task is not.
    if (xQueueSend(text_queue_, &item, 0) != pdTRUE) {
        free(copy);
        ESP_LOGW(TAG, "text TX queue full, dropping frame (len=%zu)", len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t WebSocketClient::sendMessage(cJSON* msg)
{
    if (!msg) return ESP_ERR_INVALID_ARG;

    char* json = cJSON_PrintUnformatted(msg);
    cJSON_Delete(msg);
    if (!json) return ESP_ERR_NO_MEM;

    esp_err_t err = sendText(json, strlen(json));
    cJSON_free(json);
    return err;
}

esp_err_t WebSocketClient::sendBinary(const uint8_t* data, size_t len)
{
    if (!client_ || !connected_ || !tx_buffer_) return ESP_FAIL;

    size_t space = xStreamBufferSpacesAvailable(tx_buffer_);
    if (space < len) return ESP_ERR_NO_MEM;

    size_t sent = xStreamBufferSend(tx_buffer_, data, len, 0);
    return (sent == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t WebSocketClient::sendBinaryWhole(const uint8_t* data, size_t len)
{
    if (!client_ || !connected_ || !data || len == 0) return ESP_FAIL;

    // Send directly as a single WS frame instead of going through tx_buffer_,
    // whose batching would chop the image into 2048-byte frames. The esp
    // websocket client serializes sends with its own recursive lock, so this is
    // safe to call alongside the TX task's send_bin().
    int sent = esp_websocket_client_send_bin(
        client_, (const char*)data, len, pdMS_TO_TICKS(1000));
    if (sent < 0) {
        ESP_LOGW(TAG, "sendBinaryWhole failed (len=%zu)", len);
        return ESP_FAIL;
    }
    if ((size_t)sent != len) {
        ESP_LOGW(TAG, "sendBinaryWhole partial: %d/%zu", sent, len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t WebSocketClient::sendAudioFrame(uint16_t seq, const uint8_t* opus_data, size_t opus_len)
{
    if (!opus_data || opus_len == 0) return ESP_ERR_INVALID_ARG;

    uint8_t header[5];
    header[0] = ws::AUDIO_UPLINK;
    header[1] = (uint8_t)(seq & 0xFF);
    header[2] = (uint8_t)((seq >> 8) & 0xFF);
    header[3] = (uint8_t)(opus_len & 0xFF);
    header[4] = (uint8_t)((opus_len >> 8) & 0xFF);

    size_t total = 5 + opus_len;
    if (!tx_buffer_ || xStreamBufferSpacesAvailable(tx_buffer_) < total) {
        return ESP_ERR_NO_MEM;
    }

    xStreamBufferSend(tx_buffer_, header, 5, 0);
    xStreamBufferSend(tx_buffer_, opus_data, opus_len, 0);
    return ESP_OK;
}

// === TX buffer ===

void WebSocketClient::waitTxDrain(uint32_t timeout_ms)
{
    if (!tx_buffer_) return;

    uint32_t elapsed = 0;
    while (elapsed < timeout_ms) {
        if (xStreamBufferBytesAvailable(tx_buffer_) == 0) return;
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed += 10;
    }
    ESP_LOGW(TAG, "TX drain timeout (%lu bytes left)",
             (unsigned long)xStreamBufferBytesAvailable(tx_buffer_));
    xStreamBufferReset(tx_buffer_);
}

size_t WebSocketClient::getTxFreeSpace() const
{
    return tx_buffer_ ? xStreamBufferSpacesAvailable(tx_buffer_) : 0;
}

void WebSocketClient::resetTxBuffer()
{
    if (tx_buffer_) xStreamBufferReset(tx_buffer_);
}

// === TX Task ===

void WebSocketClient::txTaskEntry(void* arg)
{
    static_cast<WebSocketClient*>(arg)->txLoop();
}

void WebSocketClient::txLoop()
{
    ESP_LOGI(TAG, "TX task started");

    constexpr size_t BATCH_BUF_SIZE = server_cfg::WS_TX_BATCH_SIZE;
    uint8_t* buf = (uint8_t*)malloc(BATCH_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "TX: failed to allocate batch buffer");
        vTaskDelete(nullptr);
        return;
    }

    int consecutive_failures = 0;

    // Every batch from tx_buffer_ is mic audio (the only producer is the SPI
    // AUDIO_UPLINK relay). Tag each WS message with 0xAA so the server can
    // route binary frames unambiguously — without the tag, an Opus frame whose
    // length byte happens to be 0xAD was misrouted as a camera image chunk.
    constexpr uint8_t WS_AUDIO_UPLINK_TAG = 0xAA;

    while (run_tx_task_) {
        if (!connected_ || !client_ || !tx_buffer_) {
            if (tx_buffer_ && !connected_) {
                xStreamBufferReset(tx_buffer_);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t batch_len = xStreamBufferReceive(tx_buffer_, buf + 1, BATCH_BUF_SIZE - 1, pdMS_TO_TICKS(20));
        if (batch_len == 0) continue;

        if (!connected_ || !client_) continue;

        buf[0] = WS_AUDIO_UPLINK_TAG;
        batch_len += 1;
        int sent = esp_websocket_client_send_bin(client_, (const char*)buf, batch_len, pdMS_TO_TICKS(200));
        if (sent < 0) {
            consecutive_failures++;
            if (consecutive_failures <= 3) {
                ESP_LOGW(TAG, "TX: send_bin failed (%d), batch=%zu", consecutive_failures, batch_len);
            }
            if (consecutive_failures >= 5) {
                ESP_LOGW(TAG, "TX: %d failures, flushing", consecutive_failures);
                xStreamBufferReset(tx_buffer_);
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        } else {
            consecutive_failures = 0;
        }
    }

    free(buf);
    ESP_LOGI(TAG, "TX task stopped");
    vTaskDelete(nullptr);
}

// === Text TX Task ===

void WebSocketClient::textTaskEntry(void* arg)
{
    static_cast<WebSocketClient*>(arg)->textLoop();
}

void WebSocketClient::textLoop()
{
    ESP_LOGI(TAG, "Text TX task started");

    while (run_text_task_) {
        TxTextItem item;
        if (xQueueReceive(text_queue_, &item, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        if (connected_ && client_) {
            int sent = esp_websocket_client_send_text(
                client_, item.data, item.len, pdMS_TO_TICKS(1000));
            if (sent != (int)item.len) {
                ESP_LOGW(TAG, "text TX incomplete: %d/%u", sent, item.len);
            }
        }
        free(item.data);
    }

    // Drain on shutdown so queued copies don't leak.
    TxTextItem item;
    while (xQueueReceive(text_queue_, &item, 0) == pdTRUE) free(item.data);

    ESP_LOGI(TAG, "Text TX task stopped");
    vTaskDelete(nullptr);
}

// === Event handler ===

void WebSocketClient::eventHandlerStatic(void* handler_args, esp_event_base_t base,
                                          int32_t event_id, void* event_data)
{
    auto* self = static_cast<WebSocketClient*>(handler_args);
    self->eventHandler(base, event_id, (esp_websocket_event_data_t*)event_data);
}

void WebSocketClient::eventHandler(esp_event_base_t base, int32_t event_id,
                                    esp_websocket_event_data_t* data)
{
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WS connected");
        connected_ = true;
        authenticated_ = false;
        if (state_cb_) state_cb_(true);

        if (sendAuthMessage() != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send auth message");
            if (auth_cb_) auth_cb_(false, last_close_code_);
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (!data) break;

        // Continuation frames (op_code 0x0) inherit the opcode of the
        // message they continue; chunked reads of one large frame keep the
        // original opcode. Track it so fragmented binary keeps flowing to
        // binary_cb_ with correct offset/total info.
        if (data->op_code == 0x1 || data->op_code == 0x2) {
            last_data_opcode_ = data->op_code;
        }

        if (last_data_opcode_ == 0x1 && (data->op_code == 0x1 || data->op_code == 0x0)) {  // TEXT
            // Check if this is auth_result
            if (!authenticated_) {
                ws::ParsedMessage msg = ws::parseMessage(
                    (const char*)data->data_ptr, data->data_len);
                if (msg.valid && msg.type == ws::MsgType::AUTH_RESULT) {
                    cJSON* status = cJSON_GetObjectItem(msg.payload, "status");
                    if (status && cJSON_IsString(status) &&
                        strcmp(status->valuestring, "ok") == 0) {
                        authenticated_ = true;
                        ESP_LOGI(TAG, "Authentication successful");
                        if (auth_event_) xEventGroupSetBits(auth_event_, AUTH_SUCCESS_BIT);
                        if (auth_cb_) auth_cb_(true, 0);
                    } else {
                        const char* reason = "unknown";
                        cJSON* reason_item = cJSON_GetObjectItem(msg.payload, "reason");
                        if (reason_item && cJSON_IsString(reason_item)) {
                            reason = reason_item->valuestring;
                        }
                        ESP_LOGW(TAG, "Authentication rejected: %s", reason);
                        // Server-side rejection (bad/revoked token), not a
                        // transport drop — flag it so NetworkManager falls back
                        // to BLE instead of retrying a doomed handshake forever.
                        auth_rejected_ = true;
                        if (auth_event_) xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
                        if (auth_cb_) auth_cb_(false, last_close_code_);
                    }
                    if (msg.root) cJSON_Delete(msg.root);
                    break;
                }
                if (msg.root) cJSON_Delete(msg.root);
            }

            if (text_cb_) {
                text_cb_((const char*)data->data_ptr, data->data_len);
            }
        } else if (last_data_opcode_ == 0x2 &&
                   (data->op_code == 0x2 || data->op_code == 0x0)) {  // BINARY
            if (binary_cb_) {
                binary_cb_((const uint8_t*)data->data_ptr, data->data_len,
                           data->payload_offset, data->payload_len);
            }
        }
        break;

    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGW(TAG, "WS closed (code=%d)", data ? data->op_code : -1);
        if (data) last_close_code_ = data->op_code;
        connected_ = false;
        authenticated_ = false;
        if (tx_buffer_) xStreamBufferReset(tx_buffer_);
        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
        if (!authenticated_ && auth_cb_) auth_cb_(false, last_close_code_);
        if (state_cb_) state_cb_(false);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WS disconnected");
        connected_ = false;
        authenticated_ = false;
        if (tx_buffer_) xStreamBufferReset(tx_buffer_);
        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
        if (!authenticated_ && auth_cb_) auth_cb_(false, last_close_code_);
        if (state_cb_) state_cb_(false);
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error");
        connected_ = false;
        authenticated_ = false;
        if (tx_buffer_) xStreamBufferReset(tx_buffer_);
        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
        if (!authenticated_ && auth_cb_) auth_cb_(false, last_close_code_);
        if (state_cb_) state_cb_(false);
        break;

    default:
        break;
    }
}
