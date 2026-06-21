#pragma once
#include <cstddef>
#include <cstdint>

namespace server_cfg {

// WebSocket
static constexpr uint32_t WS_AUTH_TIMEOUT_MS        = 5000;
static constexpr uint32_t WS_PING_INTERVAL_SEC      = 10;
static constexpr uint32_t WS_PINGPONG_TIMEOUT_SEC   = 120;
static constexpr uint32_t WS_BUFFER_SIZE             = 4096;
static constexpr uint32_t WS_NETWORK_TIMEOUT_MS      = 10000;
static constexpr size_t   WS_TX_BUFFER_SIZE          = 32 * 1024;
static constexpr size_t   WS_TX_BATCH_SIZE           = 2048;
static constexpr uint32_t WS_TX_TASK_STACK_SIZE      = 4096;
// Text/JSON frames go through their own queue + task so heartbeat / watchdog /
// log-flush sends (which run on the shared esp_timer task) never block up to 1s
// inside esp_websocket_client_send_text. Depth covers a log-flush burst (10) +
// heartbeat + a few state updates without dropping.
static constexpr uint32_t WS_TXT_TASK_STACK_SIZE     = 4096;
static constexpr size_t   WS_TEXT_QUEUE_DEPTH        = 24;
static constexpr size_t   WS_TEXT_MAX_LEN            = 4096;
static constexpr uint32_t HEARTBEAT_INTERVAL_MS      = 30000;

// Inter-MCU audio bridge
static constexpr size_t   SPI_DL_AUDIO_BUFFER_SIZE   = 24 * 1024;

// Reconnect
static constexpr uint32_t RECONNECT_BASE_DELAY_MS    = 1000;
static constexpr uint32_t RECONNECT_MAX_DELAY_MS     = 30000;
static constexpr uint8_t  RECONNECT_MAX_ATTEMPTS     = 10;

// OTA
static constexpr uint32_t OTA_CHECK_INTERVAL_MS      = 3600000; // 1 hour
static constexpr size_t   OTA_CHUNK_SIZE              = 4096;
static constexpr uint32_t OTA_DOWNLOAD_TIMEOUT_MS     = 300000;  // 5 min

// Heartbeat offline detection (server-side: 90s = 3 missed heartbeats)
static constexpr uint32_t OFFLINE_THRESHOLD_MS        = 90000;

// TCP keepalive
static constexpr uint32_t TCP_KEEPALIVE_IDLE_SEC      = 5;
static constexpr uint32_t TCP_KEEPALIVE_INTERVAL_SEC  = 5;
static constexpr uint32_t TCP_KEEPALIVE_COUNT         = 3;

// Log forwarding
static constexpr uint32_t LOG_FLUSH_INTERVAL_MS       = 5000;
static constexpr size_t   LOG_BUFFER_SIZE              = 64;

} // namespace server_cfg
