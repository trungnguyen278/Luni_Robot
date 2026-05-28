#include "HttpClient.hpp"
#include "config/ServerConfig.hpp"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "HttpClient";

HttpClient::~HttpClient()
{
    if (client_) {
        esp_http_client_cleanup(client_);
        client_ = nullptr;
    }
}

esp_err_t HttpClient::download(const char* url, size_t expected_size,
                                ChunkCallback chunk_cb, ProgressCallback progress_cb)
{
    if (!url || !chunk_cb) return ESP_ERR_INVALID_ARG;

    aborted_ = false;

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 10000;
    config.buffer_size = server_cfg::OTA_CHUNK_SIZE;
    config.buffer_size_tx = 512;

    client_ = esp_http_client_init(&config);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client_, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP open failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client_);
        client_ = nullptr;
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client_);
    if (content_length < 0) {
        ESP_LOGE(TAG, "Failed to fetch headers");
        esp_http_client_close(client_);
        esp_http_client_cleanup(client_);
        client_ = nullptr;
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client_);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP %d", status);
        esp_http_client_close(client_);
        esp_http_client_cleanup(client_);
        client_ = nullptr;
        return ESP_FAIL;
    }

    size_t total = (content_length > 0) ? (size_t)content_length : expected_size;
    size_t downloaded = 0;

    uint8_t* buf = (uint8_t*)malloc(server_cfg::OTA_CHUNK_SIZE);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate chunk buffer");
        esp_http_client_close(client_);
        esp_http_client_cleanup(client_);
        client_ = nullptr;
        return ESP_ERR_NO_MEM;
    }

    while (!aborted_) {
        int read = esp_http_client_read(client_, (char*)buf, server_cfg::OTA_CHUNK_SIZE);
        if (read < 0) {
            ESP_LOGE(TAG, "Read error");
            err = ESP_FAIL;
            break;
        }
        if (read == 0) {
            if (esp_http_client_is_complete_data_received(client_)) {
                err = ESP_OK;
            } else {
                err = ESP_FAIL;
                ESP_LOGE(TAG, "Connection closed prematurely");
            }
            break;
        }

        chunk_cb((const uint8_t*)buf, (size_t)read);
        downloaded += read;

        if (progress_cb && total > 0) {
            progress_cb(downloaded, total);
        }
    }

    free(buf);
    esp_http_client_close(client_);
    esp_http_client_cleanup(client_);
    client_ = nullptr;

    if (aborted_) {
        ESP_LOGW(TAG, "Download aborted");
        return ESP_ERR_INVALID_STATE;
    }

    return err;
}

int HttpClient::get(const char* url, char* buf, size_t buf_size)
{
    if (!url) return -1;

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = 10000;
    config.buffer_size = 1024;

    esp_http_client_handle_t c = esp_http_client_init(&config);
    if (!c) return -1;

    esp_err_t err = esp_http_client_open(c, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(c);
        return -1;
    }

    esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);

    if (buf && buf_size > 0) {
        int read = esp_http_client_read(c, buf, buf_size - 1);
        if (read >= 0) buf[read] = '\0';
        else buf[0] = '\0';
    }

    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}
