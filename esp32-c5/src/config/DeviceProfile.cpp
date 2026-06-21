#include "DeviceProfile.hpp"
#include "AppController.hpp"

#include "system/NetworkManager.hpp"
#include "system/OtaManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/UartBridge.hpp"
#include "protocol/WsProtocol.hpp"
#include "system/BluetoothService.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"
#include "system/MotionManager.hpp"
#include "system/PowerManager.hpp"
#include "Power.hpp"

#include "config/PinConfig.hpp"

#include "TouchInput.hpp"
#include "WifiService.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdio>
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

// Mic privacy-mute button (QĐ-10). File-scope (lives past setup()). The mic is on
// the S3; this button toggles a flag and pushes SET_MIC_MUTE to the S3 over UART.
static TouchInput s_muteButton;
static bool       s_mic_muted = false;

// Servo + IMU motion controller (I2C moved here from the S3). File-scope so it
// persists after setup(). g_motion (declared in MotionManager.hpp) points at it
// so WsMessageHandler can drive servos directly from a WS MOTION command.
static MotionManager s_motion;
MotionManager* g_motion = nullptr;

// Battery monitor (ADC moved here from the S3, which had no free ADC pin).
// File-scope so the sampling timer keeps running after setup() returns.
static std::unique_ptr<PowerManager> s_power;

// ---- Voice-turn triggers (shared by the local button and the UART path) ----
// Begin a listening turn: tell the server, reset downlink, and flip interaction
// state to LISTENING. The state change is auto-pushed to S3 over UART (see
// AppController::onInteractionStateChanged) so S3 opens its microphone.
static void triggerListenStart(NetworkManager* net, SpiBridge* spi,
                               state::InputSource src = state::InputSource::BUTTON)
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
        state::InteractionState::LISTENING, src);
}

// End the listening turn: the server now runs STT+LLM+TTS, so the turn enters
// PROCESSING — NOT IDLE. The downlink gate (NetworkManager::onBinary) only
// accepts TTS while PROCESSING/SPEAKING; going IDLE here made it drop the
// whole reply. NetworkManager's watchdog returns PROCESSING to IDLE if the
// server never answers.
static void triggerListenEnd(NetworkManager* net)
{
    if (!net) return;
    if (StateManager::instance().getInteractionState() !=
        state::InteractionState::LISTENING) {
        return;  // no turn in progress (e.g. END after an ignored START)
    }
    net->waitTxDrain(1000);
    net->sendText("END");
    StateManager::instance().setInteractionState(
        state::InteractionState::PROCESSING, state::InputSource::SYSTEM);
}

static bool shouldEraseNvs()
{
    constexpr int pin = PinConfig::NVS_RESET_BTN;
    if (pin < 0) return false;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << pin;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;   // idle HIGH; a real button pulls to GND
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Fast path: not held → return immediately (don't slow every boot).
    vTaskDelay(pdMS_TO_TICKS(20));
    if (gpio_get_level((gpio_num_t)pin) != 0) {
        gpio_reset_pin((gpio_num_t)pin);
        return false;
    }

    // GUARD (R-DA-RBT-009): a single LOW read used to wipe WiFi creds + device
    // token — catastrophic if GPIO9 floats (no button/pull on the real HW) or
    // glitches at boot. Require a SUSTAINED hold: any HIGH sample aborts. Only a
    // deliberate ~3s press erases NVS. Set NVS_RESET_BTN=-1 to disable outright.
    constexpr int HOLD_MS = 3000;
    constexpr int STEP_MS = 100;
    for (int waited = 0; waited < HOLD_MS; waited += STEP_MS) {
        vTaskDelay(pdMS_TO_TICKS(STEP_MS));
        if (gpio_get_level((gpio_num_t)pin) != 0) {
            ESP_LOGW(TAG, "NVS reset aborted: GPIO%d released after %dms (need %dms hold)",
                     pin, waited, HOLD_MS);
            gpio_reset_pin((gpio_num_t)pin);
            return false;
        }
    }

    ESP_LOGW(TAG, "NVS reset CONFIRMED: GPIO%d held LOW for %dms", pin, HOLD_MS);
    gpio_reset_pin((gpio_num_t)pin);
    return true;
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
        case uart_proto::ControlCmd::START: {
            // S3-triggered turn (wake word, or a button wired on the S3).
            // Payload[0] = InputSource; legacy senders send no payload → BUTTON.
            auto src = state::InputSource::BUTTON;
            if (len >= 1 && data[0] < (uint8_t)state::InputSource::UNKNOWN) {
                src = (state::InputSource)data[0];
            }
            triggerListenStart(net_ptr, spi_ptr, src);
            break;
        }
        case uart_proto::ControlCmd::END:
            triggerListenEnd(net_ptr);
            break;
        case uart_proto::ControlCmd::SPEAK_DONE:
            // S3 finished draining TTS on the speaker → the turn is over.
            net_ptr->endSpeakingSession();
            StateManager::instance().setInteractionState(
                state::InteractionState::IDLE, state::InputSource::SYSTEM);
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
            (uint8_t)e,
            (uint8_t)StateManager::instance().getLastFailReason());
    });

    // OTA: relay state changes + download progress to S3 so its OTA screen
    // (progress bar / verifying / flashing) actually runs, and report the same
    // phase/percent to the server so the cloud can advance OTAHistory.
    OtaManager* ota_mgr = network_mgr->getOtaManager();
    StateManager::instance().subscribeOta([uart_ptr, net_ptr, ota_mgr](state::OtaState s) {
        uint8_t pct = ota_mgr ? ota_mgr->getProgress() : 0;
        uart_ptr->sendOtaStatus((uint8_t)s, pct);
        net_ptr->sendOtaProgress(pct, state::otaPhaseName(s));
    });
    if (ota_mgr) {
        ota_mgr->onProgress([uart_ptr, net_ptr](uint8_t pct) {
            uart_ptr->sendOtaStatus((uint8_t)state::OtaState::DOWNLOADING, pct);
            net_ptr->sendOtaProgress(pct, state::otaPhaseName(state::OtaState::DOWNLOADING));
        });
    }

    // Report every state change to the server (`state_update`); the server
    // caches it in Redis/DB (`last_state`) for the app. Drops harmlessly while
    // offline (sendText guards on ws_connected_).
    StateManager::instance().subscribeAll([net_ptr](const state::StateEvent& e) {
        net_ptr->sendStateUpdate(e);
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
    // 4c. MOTION (PCA9685 servos + MPU6050 IMU on I2C) — moved from S3
    // =========================================================
    // The C5 now owns the I2C motion bus. It drives servos directly on a WS
    // MOTION command (WsMessageHandler::handleMotion → g_motion->post), and
    // reports IMU events (e.g. fall) straight to the server — no UART hop.
    {
        MotionManager::Config mcfg{
            .sda         = PinConfig::I2C::SDA,
            .scl         = PinConfig::I2C::SCL,
            .i2c_port    = PinConfig::I2C::PORT,
            .scl_hz      = (uint32_t)PinConfig::I2C::FREQ_HZ,
            .pca_addr    = (uint8_t)PinConfig::I2C::PCA9685_ADDR,
            .imu_addr    = (uint8_t)PinConfig::I2C::MPU6050_ADDR,
            .pwm_freq_hz = PinConfig::I2C::PWM_FREQ_HZ,
        };
        if (s_motion.init(mcfg)) {
            // IMU events → server as a small JSON log entry (same shape the S3
            // used to forward; now sent directly from the MCU that owns the IMU).
            s_motion.setImuEventCb([net_ptr](const char* evt, float pitch, float roll) {
                char buf[96];
                int n = snprintf(buf, sizeof(buf),
                    "{\"type\":\"imu\",\"evt\":\"%s\",\"pitch\":%.0f,\"roll\":%.0f}",
                    evt, pitch, roll);
                if (n > 0) net_ptr->sendText(buf, (size_t)n);
            });
            s_motion.start();
            g_motion = &s_motion;   // publish to WsMessageHandler
            ESP_LOGI(TAG, "MotionManager OK (servos%s)",
                     s_motion.imuReady() ? " + IMU" : ", no IMU");
        } else {
            ESP_LOGW(TAG, "MotionManager init failed (servos unavailable)");
        }
    }

    // =========================================================
    // 4d. POWER / BATTERY (ADC) — moved from S3 (no free ADC pin there)
    // =========================================================
    // The C5 reads the battery on ADC1 (GPIO1) and REPORTS %+state to the S3 over
    // UART (status-bar icon) and to the server. If no divider is wired the driver
    // reads floating → reports 255 → S3 hides the icon (never a fake %).
    if (PinConfig::Battery::ADC_CHANNEL >= 0) {
        auto power_drv = std::make_unique<Power>(
            (adc_channel_t)PinConfig::Battery::ADC_CHANNEL,
            (gpio_num_t)PinConfig::Battery::CHG_STATUS,
            (gpio_num_t)PinConfig::Battery::CHG_FULL,
            PinConfig::Battery::R1, PinConfig::Battery::R2);

        PowerManager::Config pcfg{};
        s_power = std::make_unique<PowerManager>(std::move(power_drv), pcfg);
        if (s_power->init()) {
            s_power->setReportCb([uart_ptr, net_ptr](uint8_t percent, state::PowerState st) {
                // → S3 for the status-bar battery icon.
                uint8_t payload[2] = { percent, (uint8_t)st };
                uart_ptr->sendDeviceCmd(uart_proto::ControlCmd::SET_BATTERY, payload, 2);
                // → server (small JSON, like IMU events).
                char buf[64];
                int n = snprintf(buf, sizeof(buf),
                    "{\"type\":\"battery\",\"pct\":%u,\"state\":%u}",
                    percent, (unsigned)st);
                if (n > 0) net_ptr->sendText(buf, (size_t)n);
            });
            s_power->start();
            ESP_LOGI(TAG, "PowerManager OK (battery ADC on GPIO%d)",
                     PinConfig::Battery::ADC_GPIO);
        } else {
            ESP_LOGW(TAG, "PowerManager init failed");
            s_power.reset();
        }
    } else {
        ESP_LOGI(TAG, "Battery ADC disabled (Battery::ADC_CHANNEL=-1)");
    }

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
    // 4c. MIC PRIVACY-MUTE BUTTON (QĐ-10)
    // =========================================================
    // The always-on mic (wake word) lives on the S3, but the S3-CAM board has no
    // free GPIO — the C5 does. Each PRESS toggles mute and pushes SET_MIC_MUTE to
    // the S3, which cuts its I2S input + shows the status-bar mic icon. uart_ptr
    // stays valid after attachModules (the object lives in AppController).
    if (PinConfig::MUTE_BUTTON >= 0) {
        TouchInput::Config mute_cfg{
            .pin = (gpio_num_t)PinConfig::MUTE_BUTTON,
            .active_low = true,
            .long_press_ms = 1500,
            .debounce_ms = 40,
        };
        if (s_muteButton.init(mute_cfg)) {
            s_muteButton.onEvent([uart_ptr](TouchInput::Event e) {
                if (e != TouchInput::Event::PRESS) return;   // toggle on press only
                s_mic_muted = !s_mic_muted;
                uint8_t payload = s_mic_muted ? 1 : 0;
                uart_ptr->sendDeviceCmd(uart_proto::ControlCmd::SET_MIC_MUTE, &payload, 1);
                ESP_LOGI(TAG, "Mic %s (privacy mute button)", s_mic_muted ? "MUTED" : "LIVE");
            });
            s_muteButton.start();
            ESP_LOGI(TAG, "Mic-mute button ready on GPIO%d", PinConfig::MUTE_BUTTON);
        } else {
            ESP_LOGW(TAG, "Mic-mute button init failed (non-fatal)");
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
