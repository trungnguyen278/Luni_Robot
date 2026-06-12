#include "DeviceProfile.hpp"
#include "AppController.hpp"

#include "system/NetworkManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"
#include "protocol/WsProtocol.hpp"
#include "system/BluetoothService.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"

#include "config/PinConfig.hpp"

#include "TouchInput.hpp"
#include "WifiService.hpp"
#include <cstring>
#include <cstdlib>
#include <utility>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_system.h"
#include "driver/gpio.h"

static const char* TAG = "DeviceProfile";

// User push-to-talk button (moved here from S3). File-scope so it lives for the
// program lifetime after DeviceProfile::setup() returns.
static TouchInput s_userButton;

// ---- Voice-turn triggers (shared by the button and the legacy UART path) ----
// Begin a listening turn: tell the server, reset downlink, and flip interaction
// state to LISTENING. The state change is auto-pushed to S3 over UART (see
// AppController::onInteractionStateChanged) so S3 opens its microphone.
static void triggerListenStart(NetworkManager* net, SpiBridge* spi)
{
    if (!net) return;
    // Mirror the old S3 guard: only start a turn when actually online.
    if (StateManager::instance().getConnectionState() != state::ConnectionState::ONLINE) {
        ESP_LOGW(TAG, "Listen START ignored: not ONLINE");
        return;
    }
    net->endSpeakingSession();
    if (spi) spi->resetDownlink();
    net->sendText("START");
    StateManager::instance().setInteractionState(
        state::InteractionState::LISTENING, state::InputSource::BUTTON);
}

static void triggerListenEnd(NetworkManager* net)
{
    if (!net) return;
    net->waitTxDrain(1000);
    net->sendText("END");
    StateManager::instance().setInteractionState(
        state::InteractionState::IDLE, state::InputSource::BUTTON);
}

static bool shouldEraseNvs()
{
    constexpr int pin = PinConfig::NVS_RESET_BTN;
    if (pin < 0) return false;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    vTaskDelay(pdMS_TO_TICKS(50));

    bool pressed = gpio_get_level((gpio_num_t)pin) == 0;
    if (pressed) {
        ESP_LOGW(TAG, "NVS reset button detected (GPIO%d LOW)", pin);
    }

    gpio_reset_pin((gpio_num_t)pin);
    return pressed;
}

namespace user_cfg {
    struct UserSettings {
        std::string device_name = "Luni";
        uint8_t  volume = 30;
        std::string wifi_ssid;
        std::string wifi_pass;
        std::string ws_url;
        std::string user_id;
        std::string device_token;
    };

    static std::string get_string(nvs_handle_t h, const char* key) {
        size_t required = 0;
        if (nvs_get_str(h, key, nullptr, &required) != ESP_OK || required == 0) return {};
        std::string tmp; tmp.resize(required);
        if (nvs_get_str(h, key, tmp.data(), &required) != ESP_OK) return {};
        if (!tmp.empty() && tmp.back() == '\0') tmp.pop_back();
        return tmp;
    }

    static uint8_t get_u8(nvs_handle_t h, const char* key, uint8_t def_val) {
        uint8_t v = def_val;
        nvs_get_u8(h, key, &v);
        return v;
    }

    static UserSettings load() {
        UserSettings cfg;
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) {
            ESP_LOGI(TAG, "NVS storage not found, using defaults");
            return cfg;
        }
        cfg.device_name  = get_string(h, "device_name");
        if (cfg.device_name.empty()) cfg.device_name = "Luni";
        cfg.wifi_ssid    = get_string(h, "ssid");
        cfg.wifi_pass    = get_string(h, "pass");
        cfg.ws_url        = get_string(h, "ws_url");
        cfg.user_id       = get_string(h, "user_id");
        cfg.device_token  = get_string(h, "device_token");
        cfg.volume        = get_u8(h, "volume", cfg.volume);
        nvs_close(h);
        return cfg;
    }
}

static std::string normalize_ws_url(std::string val) {
    while (!val.empty() && (val.front() == ' ' || val.front() == '\n')) val.erase(val.begin());
    while (!val.empty() && (val.back() == ' ' || val.back() == '\n')) val.pop_back();
    if (val.empty()) return {};
    if (val.rfind("ws://", 0) == 0 || val.rfind("wss://", 0) == 0) {
        if (val.find('/', val.find("://") + 3) == std::string::npos) val += "/ws";
        return val;
    }
    return "ws://" + val + "/ws";
}

bool DeviceProfile::setup(AppController& app)
{
    ESP_LOGI(TAG, "DeviceProfile setup begin (ESP32-C5), free heap: %lu",
             (unsigned long)esp_get_free_heap_size());

    if (shouldEraseNvs()) {
        ESP_LOGW(TAG, "*** Erasing all NVS data (reset button held) ***");
        nvs_flash_erase();
    }
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    auto user = user_cfg::load();

    // =========================================================
    // 1. PROVISIONING PREP (only when WiFi creds or device token missing)
    //    Scans WiFi here so the app can show a real network list over BLE.
    //    BLE provisioning is driven later by the NetworkManager state machine.
    // =========================================================
    esp_netif_init();
    esp_event_loop_create_default();

    const bool has_wifi = !user.wifi_ssid.empty();
    const bool has_device_token = !user.device_token.empty();
    const bool needs_provisioning = !has_wifi || (has_wifi && !has_device_token);
    if (needs_provisioning) {
        ESP_LOGW(TAG, "Provisioning required: wifi=%s token=%s",
                 has_wifi ? "set" : "missing",
                 has_device_token ? "set" : "missing");

        esp_netif_t* scan_netif = esp_netif_create_default_wifi_sta();
        wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
        esp_wifi_init(&wifi_cfg);
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
        vTaskDelay(pdMS_TO_TICKS(500));

        wifi_scan_config_t scan_cfg = {};
        esp_wifi_scan_start(&scan_cfg, true);
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        std::vector<wifi_ap_record_t> ap_records(ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_records.data());

        std::vector<WifiInfo> networks;
        for (int i = 0; i < ap_count; i++) {
            WifiInfo info;
            info.ssid = (const char*)ap_records[i].ssid;
            info.rssi = ap_records[i].rssi;
            networks.push_back(info);
        }

        esp_wifi_stop();
        esp_wifi_deinit();
        esp_netif_destroy(scan_netif);

        BluetoothService::ConfigData ble_cfg;
        ble_cfg.device_name  = user.device_name;
        ble_cfg.volume       = user.volume;
        ble_cfg.ssid         = user.wifi_ssid;
        ble_cfg.pass         = user.wifi_pass;
        ble_cfg.ws_url       = user.ws_url;
        ble_cfg.user_id      = user.user_id;
        ble_cfg.device_token = user.device_token;

        // Hand the scanned list + current settings to the AppController. BLE
        // provisioning itself is no longer blocking here — it is started later by
        // the NetworkManager connection state machine (when WiFi creds or device
        // token are missing), which notifies the S3 to show the provisioning
        // screen and applies the received config live. See
        // AppController::startBleProvisioning / NetworkManager::applyProvisionedConfig.
        const unsigned scanned = (unsigned)networks.size();
        app.setProvisioningContext(std::move(networks), std::move(ble_cfg));
        ESP_LOGI(TAG, "Provisioning context ready (scanned %u networks); "
                      "BLE will start via NetworkManager", scanned);
    }

    // =========================================================
    // 2. NETWORK (WiFi + WebSocket only — no MQTT)
    // =========================================================
    auto network_mgr = std::make_unique<NetworkManager>();

    // Production endpoint is behind Cloudflare (TLS only). Default to wss://
    // so an unprovisioned unit still reaches the server; the app may override
    // this via BLE provisioning (see Luni_App AppConfig.defaultDeviceWsUrl).
    const std::string default_ws = "wss://lunirobot.io.vn/ws/device";

    NetworkManager::Config net_cfg{};
    net_cfg.wifi_ssid    = user.wifi_ssid;
    net_cfg.wifi_pass    = user.wifi_pass;
    net_cfg.ws_url       = normalize_ws_url(user.ws_url.empty() ? default_ws : user.ws_url);
    net_cfg.device_name  = user.device_name;
    net_cfg.device_token = user.device_token;

    if (!network_mgr->init(net_cfg)) {
        ESP_LOGE(TAG, "NetworkManager init failed");
        return false;
    }

    // =========================================================
    // 3. SPI BRIDGE (Communication with S3 via SPI2 slave)
    // =========================================================
    auto spi_bridge = std::make_unique<SpiBridge>();
    SpiBridge::Config spi_cfg{
        .pin_mosi      = (gpio_num_t)PinConfig::SPI_MOSI,
        .pin_miso      = (gpio_num_t)PinConfig::SPI_MISO,
        .pin_sclk      = (gpio_num_t)PinConfig::SPI_SCLK,
        .pin_cs        = (gpio_num_t)PinConfig::SPI_CS,
        .pin_handshake = (gpio_num_t)PinConfig::SPI_HANDSHAKE,
    };

    if (!spi_bridge->init(spi_cfg)) {
        ESP_LOGE(TAG, "SpiBridge init failed");
        return false;
    }

    network_mgr->setSpiBridge(spi_bridge.get());

    NetworkManager* net_ptr = network_mgr.get();
    spi_bridge->setUplinkSpaceQuery([net_ptr]() -> size_t {
        return net_ptr->getWsTxFreeSpace();
    });
    spi_bridge->onAudioUplink([net_ptr](const uint8_t* data, size_t len) {
        net_ptr->sendBinary(data, len);
    });

    // =========================================================
    // 4. UART BRIDGE (Control/status with S3 via UART1)
    // =========================================================
    auto uart_bridge = std::make_unique<UartBridge>();
    UartBridge::Config uart_cfg{
        .pin_tx = (gpio_num_t)PinConfig::UART_TX,
        .pin_rx = (gpio_num_t)PinConfig::UART_RX,
        .baud_rate = PinConfig::UART_BAUD,
    };

    if (!uart_bridge->init(uart_cfg)) {
        ESP_LOGE(TAG, "UartBridge init failed");
        return false;
    }

    network_mgr->setUartBridge(uart_bridge.get());

    SpiBridge* spi_ptr = spi_bridge.get();
    uart_bridge->onControlCmd([net_ptr, spi_ptr](uart_proto::ControlCmd cmd, const uint8_t* data, size_t len) {
        switch (cmd) {
        case uart_proto::ControlCmd::START:
            // Legacy path (button now lives on C5 — see s_userButton below).
            triggerListenStart(net_ptr, spi_ptr);
            break;
        case uart_proto::ControlCmd::END:
            triggerListenEnd(net_ptr);
            break;
        case uart_proto::ControlCmd::SET_VOLUME:
            if (len >= 1) {
                ESP_LOGI(TAG, "Volume command from S3: %d", data[0]);
            }
            break;
        case uart_proto::ControlCmd::REBOOT:
            esp_restart();
            break;
        case uart_proto::ControlCmd::BOOT_DONE:
            ESP_LOGI(TAG, "S3 boot complete, starting network");
            AppController::instance().startNetwork();
            break;
        case uart_proto::ControlCmd::WIFI_CONFIG: {
            if (len < 2) break;
            size_t pos = 0;
            uint8_t ssid_len = data[pos++];
            if (pos + ssid_len > len) break;
            std::string ssid(reinterpret_cast<const char*>(&data[pos]), ssid_len);
            pos += ssid_len;
            if (pos >= len) break;
            uint8_t pass_len = data[pos++];
            if (pos + pass_len > len) break;
            std::string pass(reinterpret_cast<const char*>(&data[pos]), pass_len);
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
                nvs_set_str(h, "ssid", ssid.c_str());
                nvs_set_str(h, "pass", pass.c_str());
                nvs_commit(h);
                nvs_close(h);
            }
            net_ptr->setCredentials(ssid, pass);
            break;
        }
        default:
            break;
        }
    });

    // Forward state changes to S3 via UART
    UartBridge* uart_ptr = uart_bridge.get();
    StateManager::instance().subscribeEmotion([uart_ptr](state::EmotionState e) {
        uart_ptr->sendStatusUpdate(
            (uint8_t)StateManager::instance().getInteractionState(),
            (uint8_t)StateManager::instance().getConnectionState(),
            (uint8_t)StateManager::instance().getSystemState(),
            (uint8_t)e);
    });

    // Forward LOG_ENTRY from S3 to server via WS
    uart_bridge->onLogEntry([net_ptr](const uint8_t* data, size_t len) {
        net_ptr->sendText(reinterpret_cast<const char*>(data), len);
    });

    // Stream each camera JPEG chunk to the server as its own WS frame, tagged
    // 0xAD (image-chunk uplink). The C5 has no PSRAM and can't hold a whole
    // higher-res JPEG (a 38KB malloc failed), so we forward chunk-by-chunk and
    // let the server reassemble. Each chunk MUST be one WS frame (sendBinary's
    // batching would merge/split them and corrupt the stream), hence
    // sendBinaryWhole. payload = [seq:2][flags:1][data]; we just prepend the tag.
    uart_bridge->onImageChunk([net_ptr](const uint8_t* payload, uint8_t len) {
        namespace im = uart_proto::image;
        const uint16_t seq   = (uint16_t)(payload[0] | (payload[1] << 8));
        const uint8_t  flags = payload[2];

        uint8_t buf[1 + uart_proto::MAX_PAYLOAD];
        buf[0] = ws::IMAGE_CHUNK_UPLINK;   // 0xAD
        memcpy(buf + 1, payload, len);
        const esp_err_t err = net_ptr->sendBinaryWhole(buf, len + 1);

        // Diagnostics: count chunks/bytes per image, flag any send failure or
        // sequence gap (a gap means UART chunks were dropped — likely RX starved
        // while a WS send blocked). Logged at FIRST/LAST so it's not too chatty.
        static uint32_t s_count = 0, s_bytes = 0, s_fail = 0;
        static uint16_t s_expect = 0;
        if (flags & im::FLAG_FIRST) { s_count = 0; s_bytes = 0; s_fail = 0; s_expect = seq; }
        if (seq != s_expect) {
            ESP_LOGW(TAG, "img chunk seq gap: got %u expected %u", seq, s_expect);
            s_expect = seq;
        }
        s_expect++;
        s_count++;
        s_bytes += len + 1;
        if (err != ESP_OK) { s_fail++; }
        if (flags & im::FLAG_LAST) {
            ESP_LOGI(TAG, "img streamed: %u chunks, %u WS bytes, %u send-fails",
                     (unsigned)s_count, (unsigned)s_bytes, (unsigned)s_fail);
        }
    });

    // =========================================================
    // 4b. USER PUSH-TO-TALK BUTTON (moved from S3)
    // =========================================================
    // net_ptr / spi_ptr stay valid after attachModules (objects live in
    // AppController; only ownership moves). PRESS starts a listening turn,
    // RELEASE ends it. The interaction-state change is pushed to S3 over UART.
    if (PinConfig::USER_BUTTON >= 0) {
        TouchInput::Config btn_cfg{
            .pin = (gpio_num_t)PinConfig::USER_BUTTON,
            .active_low = true,
            .long_press_ms = 1500,
            .debounce_ms = 30,
        };
        if (s_userButton.init(btn_cfg)) {
            s_userButton.onEvent([net_ptr, spi_ptr](TouchInput::Event e) {
                if (e == TouchInput::Event::PRESS)   triggerListenStart(net_ptr, spi_ptr);
                if (e == TouchInput::Event::RELEASE) triggerListenEnd(net_ptr);
            });
            s_userButton.start();
            ESP_LOGI(TAG, "User button ready on GPIO%d (push-to-talk)", PinConfig::USER_BUTTON);
        } else {
            ESP_LOGW(TAG, "User button init failed (non-fatal)");
        }
    }

    // =========================================================
    // 5. ATTACH MODULES -> APP CONTROLLER
    // =========================================================
    app.attachModules(
        std::move(network_mgr),
        std::move(spi_bridge),
        std::move(uart_bridge));

    ESP_LOGI(TAG, "DeviceProfile setup OK (ESP32-C5)");
    return true;
}
