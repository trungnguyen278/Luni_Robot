#include "WebSocketClient.hpp"
#include "WsProtocol.hpp"
#include "config/ServerConfig.hpp"
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
        tx_buffer_ = xStreamBufferCreate(48 * 1024, 1);
    }
    if (!auth_event_) {
        auth_event_ = xEventGroupCreate();
    }
}

esp_err_t WebSocketClient::connect(const char* url, const char* device_token)
{
    if (!url || !device_token) return ESP_ERR_INVALID_ARG;

    url_ = url;
    device_token_ = device_token;
    authenticated_ = false;
    last_close_code_ = 0;

    if (client_) {
        ESP_LOGW(TAG, "Already connected, closing old instance");
        disconnect();
    }

    esp_websocket_client_config_t cfg = {};
    cfg.uri = url_.c_str();
    cfg.buffer_size = server_cfg::WS_BUFFER_SIZE;
    cfg.disable_auto_reconnect = true;  // We manage reconnect in NetworkManager
    cfg.ping_interval_sec = server_cfg::WS_PING_INTERVAL_SEC;
    cfg.pingpong_timeout_sec = server_cfg::WS_PINGPONG_TIMEOUT_SEC;
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = server_cfg::TCP_KEEPALIVE_IDLE_SEC;
    cfg.keep_alive_interval = server_cfg::TCP_KEEPALIVE_INTERVAL_SEC;
    cfg.keep_alive_count = server_cfg::TCP_KEEPALIVE_COUNT;

    client_ = esp_websocket_client_init(&cfg);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init websocket");
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
        xTaskCreate(&WebSocketClient::txTaskEntry, "ws_tx", 6144, this, 6, &tx_task_);
    }

    return ESP_OK;
}

void WebSocketClient::disconnect()
{
    run_tx_task_ = false;

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

    if (tx_buffer_) {
        xStreamBufferReset(tx_buffer_);
    }

    if (state_cb_) state_cb_(false);
}

void WebSocketClient::setDeviceInfo(const char* mac, const char* fw_version, const char* model)
{
    if (mac) mac_ = mac;
    if (fw_version) fw_version_ = fw_version;
    if (model) model_ = model;
}

// === Authentication ===

esp_err_t WebSocketClient::authenticate()
{
    if (!connected_ || !client_) return ESP_FAIL;

    cJSON* auth_msg = ws::createAuthMessage(
        device_token_.c_str(), mac_.c_str(),
        fw_version_.c_str(), model_.c_str());
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

    ESP_LOGI(TAG, "Auth message sent, waiting for result...");

    xEventGroupClearBits(auth_event_, AUTH_SUCCESS_BIT | AUTH_FAIL_BIT);
    EventBits_t bits = xEventGroupWaitBits(auth_event_,
        AUTH_SUCCESS_BIT | AUTH_FAIL_BIT,
        pdTRUE, pdFALSE,
        pdMS_TO_TICKS(server_cfg::WS_AUTH_TIMEOUT_MS));

    if (bits & AUTH_SUCCESS_BIT) {
        authenticated_ = true;
        ESP_LOGI(TAG, "Authentication successful");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Authentication failed (bits=0x%lx, close_code=%d)",
             (unsigned long)bits, last_close_code_);
    return ESP_FAIL;
}

// === Send methods ===

esp_err_t WebSocketClient::sendText(const char* json, size_t len)
{
    if (!client_ || !connected_) return ESP_FAIL;

    int sent = esp_websocket_client_send_text(client_, json, len, pdMS_TO_TICKS(1000));
    if (sent != (int)len) {
        ESP_LOGW(TAG, "sendText incomplete: %d/%zu", sent, len);
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

    constexpr size_t BATCH_BUF_SIZE = 2048;
    uint8_t* buf = (uint8_t*)malloc(BATCH_BUF_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "TX: failed to allocate batch buffer");
        vTaskDelete(nullptr);
        return;
    }

    int consecutive_failures = 0;

    while (run_tx_task_) {
        if (!connected_ || !client_ || !tx_buffer_) {
            if (tx_buffer_ && !connected_) {
                xStreamBufferReset(tx_buffer_);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        size_t batch_len = xStreamBufferReceive(tx_buffer_, buf, BATCH_BUF_SIZE, pdMS_TO_TICKS(20));
        if (batch_len == 0) continue;

        if (!connected_ || !client_) continue;

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

        // Start auth flow
        if (authenticate() != ESP_OK) {
            ESP_LOGE(TAG, "Auth failed, disconnecting");
            if (auth_cb_) auth_cb_(false, last_close_code_);
        } else {
            if (auth_cb_) auth_cb_(true, 0);
        }
        break;

    case WEBSOCKET_EVENT_DATA:
        if (!data) break;

        if (data->op_code == 0x1) {  // TEXT
            // Check if this is auth_result
            if (!authenticated_) {
                ws::ParsedMessage msg = ws::parseMessage(
                    (const char*)data->data_ptr, data->data_len);
                if (msg.valid && msg.type == ws::MsgType::AUTH_RESULT) {
                    cJSON* status = cJSON_GetObjectItem(msg.payload, "status");
                    if (status && cJSON_IsString(status) &&
                        strcmp(status->valuestring, "ok") == 0) {
                        xEventGroupSetBits(auth_event_, AUTH_SUCCESS_BIT);
                    } else {
                        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
                    }
                    if (msg.root) cJSON_Delete(msg.root);
                    break;
                }
                if (msg.root) cJSON_Delete(msg.root);
            }

            if (text_cb_) {
                text_cb_((const char*)data->data_ptr, data->data_len);
            }
        } else if (data->op_code == 0x2) {  // BINARY
            if (binary_cb_) {
                binary_cb_((const uint8_t*)data->data_ptr, data->data_len);
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
        if (state_cb_) state_cb_(false);
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "WS disconnected");
        connected_ = false;
        authenticated_ = false;
        if (tx_buffer_) xStreamBufferReset(tx_buffer_);
        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
        if (state_cb_) state_cb_(false);
        break;

    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WS error");
        connected_ = false;
        authenticated_ = false;
        if (tx_buffer_) xStreamBufferReset(tx_buffer_);
        xEventGroupSetBits(auth_event_, AUTH_FAIL_BIT);
        if (state_cb_) state_cb_(false);
        break;

    default:
        break;
    }
}
