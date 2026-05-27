#pragma once
#include <cstdint>
#include <cstddef>
#include <functional>
#include "esp_http_client.h"

class HttpClient {
public:
    using ChunkCallback = std::function<void(const uint8_t* data, size_t len)>;
    using ProgressCallback = std::function<void(size_t downloaded, size_t total)>;

    HttpClient() = default;
    ~HttpClient();

    // Chunked download for OTA firmware
    // Calls chunk_cb for each received chunk, progress_cb for progress updates.
    // Returns ESP_OK on success, error code on failure.
    esp_err_t download(const char* url, size_t expected_size,
                       ChunkCallback chunk_cb, ProgressCallback progress_cb = nullptr);

    // Simple GET request, returns HTTP status code. Response body written to buf.
    int get(const char* url, char* buf, size_t buf_size);

    void abort() { aborted_ = true; }
    bool isAborted() const { return aborted_; }

private:
    bool aborted_ = false;
    esp_http_client_handle_t client_ = nullptr;
};
