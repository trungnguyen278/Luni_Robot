#include "DeviceProfile.hpp"
#include "AppController.hpp"
#include "esp_log.h"

// System managers
#include "WakeWordDetector.hpp"
#include "config/BootSubsystems.hpp"
#include "system/AudioManager.hpp"
#include "system/CameraManager.hpp"
#include "system/DisplayManager.hpp"
#include "system/PowerManager.hpp"
#include "system/SpiBridge.hpp"
#include "system/StateManager.hpp"
#include "system/StateTypes.hpp"
#include "system/UartBridge.hpp"
#include "ui/SceneManager.hpp"


// Drivers
#include "DisplayDriver.hpp"
#include "I2SAudioInput_ICS43434.hpp"
#include "I2SAudioOutput_MAX98357.hpp"
#include "Power.hpp"
#include "TouchInput.hpp"


// Codec
#include "AudioCodec.hpp"
#include "OpusCodec.hpp"

// Pin config
#include "Version.hpp"
#include "config/PinConfig.hpp"


#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"


static const char *TAG = "DeviceProfile";

// Optional camera (lazy-init on first CAMERA_CAPTURE). Disabled unless built
// with -DLUNI_ENABLE_CAMERA (see CameraManager).
static CameraManager s_camera;

// Camera capture runs on its own task so a multi-second image burst never
// blocks the UART rx loop (which dispatches state/control). CAMERA_CAPTURE just
// notifies it. Set in setup() once the UART bridge exists. (S9-01)
static TaskHandle_t s_camera_task = nullptr;
static UartBridge *s_camera_uart = nullptr;
// Display used by the camera task to flag the on-device capture indicator
// (QĐ-10). Set once disp_raw exists; the task only reads it after a
// CAMERA_CAPTURE notify.
static DisplayManager *s_camera_disp = nullptr;

// On-device wake word ("Hey Luni") — replaces the push-to-talk button. Active
// only when built with -DLUNI_ENABLE_WAKEWORD + a trained model
// (WakeWordDetector).
static WakeWordDetector s_wakeword;

// =============================================================================
// User settings from NVS
// =============================================================================
namespace user_cfg {
struct UserSettings {
  std::string dLunice_name = "Luni";
  uint8_t volume = 60;
  uint8_t brightness = 100;
};

static std::string get_string(nvs_handle_t h, const char *key) {
  size_t required = 0;
  if (nvs_get_str(h, key, nullptr, &required) != ESP_OK || required == 0)
    return {};
  std::string tmp;
  tmp.resize(required);
  if (nvs_get_str(h, key, tmp.data(), &required) != ESP_OK)
    return {};
  if (!tmp.empty() && tmp.back() == '\0')
    tmp.pop_back();
  return tmp;
}

static uint8_t get_u8(nvs_handle_t h, const char *key, uint8_t def_val) {
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
  cfg.dLunice_name = get_string(h, "dLunice_name");
  if (cfg.dLunice_name.empty())
    cfg.dLunice_name = "Luni";
  cfg.volume = get_u8(h, "volume", cfg.volume);
  cfg.brightness = get_u8(h, "brightness", cfg.brightness);
  nvs_close(h);
  return cfg;
}
} // namespace user_cfg

// Camera capture task (S9-01): blocks on a notification, then captures one
// frame and streams it to the C5. Runs OFF the UART rx task so the multi-second
// chunk burst (paced at ~100ms/chunk) can never starve incoming state/control
// frames.
static void cameraCaptureTask(void *) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!s_camera_uart)
      continue;
    // Privacy: a silent capture is a "spy camera". Light a clear on-device cue
    // for the WHOLE capture+stream window (QĐ-10) — the always-visible
    // status-bar REC dot (robust; survives emotion changes) plus the
    // purpose-built "camera" snap scene (FOCUS/SNAP!/PHOTO).
    if (s_camera_disp)
      s_camera_disp->setCapturing(true);
    SceneManager::instance().showScene("camera", "camera-snap",
                                       SceneManager::HOLD_STICKY);
    if (!s_camera.isReady())
      s_camera.init();
    const uint8_t *buf = nullptr;
    size_t n = 0;
    uint16_t w = 0, h = 0;
    if (s_camera.captureJpeg(&buf, &n, &w, &h) && buf && n > 0) {
      s_camera_uart->sendImage(buf, n, w, h);
    }
    s_camera.releaseFrame();
    if (s_camera_disp)
      s_camera_disp->setCapturing(false);
    // Clear the sticky snap scene; the idle rotation / next state takes over.
    SceneManager::instance().showEmotion("normal", nullptr,
                                         SceneManager::HOLD_AUTO);
  }
}

// =============================================================================
// Setup
// =============================================================================
bool DeviceProfile::setup(AppController &app) {
  ESP_LOGI(TAG, "DeviceProfile setup begin (ESP32-S3), free heap: %lu",
           (unsigned long)esp_get_free_heap_size());

  // Init NVS
  esp_err_t nvs_err = nvs_flash_init();
  if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES ||
      nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvs_err = nvs_flash_init();
  }
  ESP_ERROR_CHECK(nvs_err);

  auto user = user_cfg::load();

  auto &bootData = SceneManager::instance().getSceneData();

  // =========================================================
  // 1. DISPLAY (ST7789 240x320 via SPI2)
  // =========================================================
  DisplayDriver::Config lcd_cfg{};
  lcd_cfg.spi_host = SPI2_HOST;
  lcd_cfg.pin_cs = PinConfig::LCD_CS;
  lcd_cfg.pin_dc = PinConfig::LCD_DC;
  lcd_cfg.pin_rst = PinConfig::LCD_RST;
  lcd_cfg.pin_bl = PinConfig::LCD_BL;
  lcd_cfg.pin_mosi = PinConfig::SPI_MOSI;
  lcd_cfg.pin_sclk = PinConfig::SPI_SCLK;
  lcd_cfg.spi_speed_hz =
      30 * 1000 * 1000; // 30MHz: điểm ngọt cho dây dupont (40MHz hỏng cả
                        // MOSI/SCLK → mất hình; 10MHz chạy nhưng chậm). Sau khi
                        // hàn cố định có thể thử nâng 40MHz cho UI mượt nhất.

  auto lcd_driver = std::make_unique<DisplayDriver>();
  if (!lcd_driver->init(lcd_cfg)) {
    ESP_LOGE(TAG, "DisplayDriver init failed");
    return false;
  }

  auto display_mgr = std::make_unique<DisplayManager>();
  if (!display_mgr->init(std::move(lcd_driver), 240, 320)) {
    ESP_LOGE(TAG, "DisplayManager init failed");
    return false;
  }

  display_mgr->setBrightness(user.brightness);
  display_mgr->enableStateBinding(true);
  bootData.boot_check_results[BOOT_DISPLAY] = BOOT_OK;

  // =========================================================
  // 2. POWER (Battery ADC)
  // =========================================================
  std::unique_ptr<PowerManager> power_mgr;
  if (PinConfig::BATTERY_ADC >= 0) {
    auto power_drv = std::make_unique<Power>(
        ADC1_CHANNEL_0, (gpio_num_t)PinConfig::CHG_STATUS,
        (gpio_num_t)PinConfig::CHG_FULL, PinConfig::BAT_R1, PinConfig::BAT_R2);

    PowerManager::Config pwr_cfg{};
    pwr_cfg.evaluate_interval_ms = 2000;
    pwr_cfg.critical_percent = 8.0f;
    pwr_cfg.enable_smoothing = true;

    power_mgr = std::make_unique<PowerManager>(std::move(power_drv), pwr_cfg);
    if (!power_mgr->init()) {
      ESP_LOGW(TAG, "PowerManager init failed (non-fatal)");
    }
    power_mgr->setDisplayManager(display_mgr.get());
  } else {
    ESP_LOGI(TAG, "Battery ADC not on S3 (BATTERY_ADC=-1); the C5 owns the "
                  "battery ADC now and reports %% over UART (SET_BATTERY)");
  }
  // No BOOT_POWER check here: the S3 has no local battery sensor. The battery %
  // + power state arrive from the C5 over UART (ControlCmd::SET_BATTERY,
  // handled in onDeviceCmd → DisplayManager::setBatteryPercent + StateManager
  // power), so the status-bar icon shows a REAL value when a divider is wired
  // to the C5, and is hidden (255) otherwise. POWER stays off the boot splash
  // (no S3 self- test for it). The local PowerManager block above is dormant
  // (ADC=-1).

  // =========================================================
  // 3. MAX98357 CONTROL PINS
  // =========================================================
  gpio_config_t spk_sd_cfg = {};
  spk_sd_cfg.pin_bit_mask = (1ULL << PinConfig::SPK_SD);
  spk_sd_cfg.mode = GPIO_MODE_OUTPUT;
  spk_sd_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
  spk_sd_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
  gpio_config(&spk_sd_cfg);
  gpio_set_level((gpio_num_t)PinConfig::SPK_SD, 0);

  // SPK_GAIN=-1 means GAIN is hard-wired (float=9dB), no GPIO. Guard the
  // config: (1ULL << -1) is UB and was logging "gpio: GPIO_PIN mask error"
  // every boot.
  if (PinConfig::SPK_GAIN >= 0) {
    gpio_config_t spk_gain_cfg = {};
    spk_gain_cfg.pin_bit_mask = (1ULL << PinConfig::SPK_GAIN);
    spk_gain_cfg.mode = GPIO_MODE_INPUT; // High-Z (floating) = 9dB
    spk_gain_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    spk_gain_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&spk_gain_cfg);
  }
  ESP_LOGI(TAG, "MAX98357 SD=LOW (muted until playback), GAIN=float (9dB)");

  // =========================================================
  // 4. I2S FULL-DUPLEX AUDIO (shared BCLK/WS for mic+speaker)
  // =========================================================
  i2s_chan_handle_t tx_chan = nullptr;
  i2s_chan_handle_t rx_chan = nullptr;

  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
      (i2s_port_t)PinConfig::I2S_NUM, I2S_ROLE_MASTER);
  // DMA buffer sizing from AudioConfig (tunable). Larger buffer absorbs Opus
  // decode jitter and prevents speaker underrun pops.
  chan_cfg.dma_desc_num = AudioConfig::DMA_DESC_NUM;
  chan_cfg.dma_frame_num = AudioConfig::DMA_FRAME_NUM;
  // On TX underrun, send zeros instead of replaying stale DMA content.
  // Without this, every underrun audibly repeats the last ~90ms of audio.
  chan_cfg.auto_clear = true;

  esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, &rx_chan);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2S channel creation failed: %s", esp_err_to_name(err));
    return false;
  }

  auto audio_mgr = std::make_unique<AudioManager>();

  I2SAudioInput_ICS43434::Config mic_cfg{
      .pin_bclk = (gpio_num_t)PinConfig::I2S_BCLK,
      .pin_ws = (gpio_num_t)PinConfig::I2S_WS,
      .pin_din = (gpio_num_t)PinConfig::I2S_DIN,
      .sample_rate = AudioConfig::SAMPLE_RATE};
  auto mic = std::make_unique<I2SAudioInput_ICS43434>(rx_chan, mic_cfg);

  I2SAudioOutput_MAX98357::Config spk_cfg{
      .pin_bclk = (gpio_num_t)PinConfig::I2S_BCLK,
      .pin_ws = (gpio_num_t)PinConfig::I2S_WS,
      .pin_dout = (gpio_num_t)PinConfig::I2S_DOUT,
      .pin_sd = (gpio_num_t)PinConfig::SPK_SD,
      .sample_rate = AudioConfig::SAMPLE_RATE};
  auto speaker = std::make_unique<I2SAudioOutput_MAX98357>(tx_chan, spk_cfg);
  speaker->init();
  speaker->setVolume(user.volume);

  auto codec = std::make_unique<OpusCodec>();
  ESP_LOGI(TAG, "Using Opus codec");

  audio_mgr->setInput(std::move(mic));
  audio_mgr->setOutput(std::move(speaker));
  audio_mgr->setCodec(std::move(codec));

  if (!audio_mgr->init()) {
    ESP_LOGE(TAG, "AudioManager init failed");
    return false;
  }

  i2s_channel_enable(tx_chan);
  ESP_LOGI(TAG, "I2S TX enabled (clock source for full-duplex)");
  bootData.boot_check_results[BOOT_AUDIO] = BOOT_OK;

  // NOTE: the motion subsystem (PCA9685 servos + MPU6050 IMU) MOVED to the C5
  // along with the I2C bus (its pins became the SPI3 audio lane). The C5 now
  // drives servos on a WS MOTION command and reports IMU events directly. See
  // esp32-c5 DeviceProfile/MotionManager/WsMessageHandler.

  // =========================================================
  // 4c. WAKE WORD ("Hey Luni") — the only voice trigger (no button on the board)
  // =========================================================
  // On detection, enter LISTENING with InputSource::WAKEWORD: this opens the
  // mic uplink and makes AppController send START(WAKEWORD) to C5, which tells
  // the server a turn began. Active only with -DLUNI_ENABLE_WAKEWORD + the
  // trained model. The mic tap below feeds raw 16 kHz PCM to the detector
  // while idle (AudioManager keeps the mic always-on once a tap is installed).
  s_wakeword.onDetected([]() {
    auto &sm = StateManager::instance();
    if (sm.getConnectionState() != state::ConnectionState::ONLINE)
      return;
    if (sm.getInteractionState() == state::InteractionState::LISTENING)
      return;
    sm.setInteractionState(state::InteractionState::LISTENING,
                           state::InputSource::WAKEWORD);
  });
  if (s_wakeword.init()) {
    audio_mgr->setWakeWordTap(
        [](const int16_t *pcm, size_t n) { s_wakeword.feed(pcm, n); });
    ESP_LOGI(
        TAG,
        "Wake word READY — mic tap wired, always-on listening for 'Hey Luni'");
  } else {
    // CRITICAL: the wake word is the ONLY voice trigger (no button on either
    // MCU — USER_BUTTON=-1). If it fails to load, the device cannot start a
    // voice turn at all. There is no fallback by design (no QĐ for a button),
    // so make this loud. (R-DA-RBT-003 / USER-007)
    ESP_LOGE(TAG,
             "*** NO VOICE TRIGGER *** wake word inactive (disabled or "
             "model load failed) and no button on board — device cannot "
             "begin a voice turn. Re-flash with -DLUNI_ENABLE_WAKEWORD + a "
             "valid model, or wire a push-to-talk button on the C5.");
  }

  // =========================================================
  // 5. SPI BRIDGE (Communication with C5 via SPI3)
  // =========================================================
  std::unique_ptr<SpiBridge> spi_bridge;
  if (PinConfig::SPI3_MOSI >= 0) {
    spi_bridge = std::make_unique<SpiBridge>();
    SpiBridge::Config spi_cfg{
        .pin_mosi = (gpio_num_t)PinConfig::SPI3_MOSI,
        .pin_miso = (gpio_num_t)PinConfig::SPI3_MISO,
        .pin_sclk = (gpio_num_t)PinConfig::SPI3_SCLK,
        .pin_cs = (gpio_num_t)PinConfig::SPI3_CS,
        .pin_handshake = (gpio_num_t)PinConfig::SPI3_HANDSHAKE,
        // 8 MHz: these SPI3 pins are all GPIO-matrix routed (none are native
        // IOMUX SPI pins) and run over dupont wires, so start conservative —
        // the hpp default's documented-reliable rate. Raise after HW bring-up.
        .clock_speed_hz = 8 * 1000 * 1000 // 8 MHz
    };

    if (!spi_bridge->init(spi_cfg)) {
      // NON-FATAL: the display + the rest of the device MUST boot even if the
      // SPI3 audio bridge fails. Aborting setup() here would skip AppController,
      // so the display loop never starts → black screen even though the panel
      // itself is fine. Drop the bridge (audio degrades) and carry on.
      ESP_LOGE(TAG, "SpiBridge init failed — booting WITHOUT SPI3 audio bridge");
      spi_bridge.reset();
    }

    // Wire SpiBridge to AudioManager — only if it actually initialised.
    if (spi_bridge) {
    audio_mgr->setSpiBridge(spi_bridge.get());

    // SPI downlink: C5 sends frame-aligned opus data.
    StreamBufferHandle_t spk_sb = audio_mgr->getSpeakerEncodedBuffer();
    spi_bridge->onAudioDownlink([spk_sb](const uint8_t *data, size_t len) {
      if (!data || len < 3)
        return;
      auto &sm = StateManager::instance();
      auto interaction = sm.getInteractionState();
      if (interaction == state::InteractionState::LISTENING)
        return;

      if (interaction == state::InteractionState::PROCESSING) {
        sm.setInteractionState(state::InteractionState::SPEAKING,
                               state::InputSource::SERVER_COMMAND);
      } else if (interaction != state::InteractionState::SPEAKING) {
        return;
      }

      const uint8_t *frames = data + 1;
      size_t frames_len = len - 1;

      size_t space = xStreamBufferSpacesAvailable(spk_sb);
      if (space >= frames_len) {
        xStreamBufferSend(spk_sb, frames, frames_len, 0);
      } else {
        static uint32_t dl_drop_count = 0;
        dl_drop_count++;
        if (dl_drop_count <= 5 || dl_drop_count % 50 == 0) {
          ESP_LOGW("SpiBridge",
                   "DL buffer full, dropped %zu bytes (complete frames) #%lu",
                   frames_len, (unsigned long)dl_drop_count);
        }
      }
    });
    } // end if (spi_bridge)
  } else {
    ESP_LOGI(TAG, "SPI3 bridge not wired, skipping SpiBridge");
  }

  // =========================================================
  // 6. UART BRIDGE (Control/status with C5 via UART1)
  // =========================================================
  std::unique_ptr<UartBridge> uart_bridge;
  if (PinConfig::UART_TX >= 0) {
    uart_bridge = std::make_unique<UartBridge>();
    UartBridge::Config uart_cfg{
        .pin_tx = (gpio_num_t)PinConfig::UART_TX,
        .pin_rx = (gpio_num_t)PinConfig::UART_RX,
        .baud_rate = PinConfig::UART_BAUD,
    };

    if (!uart_bridge->init(uart_cfg)) {
      ESP_LOGE(TAG, "UartBridge init failed");
      return false;
    }

    UartBridge *uart_raw = uart_bridge.get();
    AudioManager *audio_raw = audio_mgr.get();

    // Camera capture now runs on its own task (S9-01) so the multi-second
    // image burst can't starve this rx loop. Wire it up here, where the UART
    // bridge that streams the image exists.
    s_camera_uart = uart_raw;
    xTaskCreatePinnedToCore(cameraCaptureTask, "CamCap", 4096, nullptr, 3,
                            &s_camera_task, 0);

    // IMU events moved to the C5 (it owns the IMU now) — no S3 forward here.
    uart_bridge->onStatusUpdate([uart_raw, audio_raw](
                                    uint8_t interaction, uint8_t connectivity,
                                    uint8_t system_state, uint8_t emotion,
                                    uint8_t fail_reason) {
      (void)emotion; // rendering uses the DEVICE_CMD SET_EMOTION key string;
                     // the enum byte here is C5 bookkeeping only (see below)
      auto &sm = StateManager::instance();

      // C5 reset detection: if S3 is already RUNNING and C5 reports OFFLINE,
      // re-send BOOT_DONE so C5 starts network without waiting for timeout
      if (sm.getSystemState() == state::SystemState::RUNNING &&
          static_cast<state::ConnectionState>(connectivity) ==
              state::ConnectionState::OFFLINE) {
        uart_raw->sendControlCmd(uart_proto::ControlCmd::BOOT_DONE);
      }

      sm.setConnectionState(static_cast<state::ConnectionState>(connectivity),
                            static_cast<state::ConnectFailReason>(fail_reason));

      // NOTE: emotion is deliberately NOT mirrored from this snapshot.
      // C5's enum only covers 16 of the server's 45 emotion keys (the
      // rest collapse to NEUTRAL), so applying it here used to clobber
      // the correct scene shown via DEVICE_CMD SET_EMOTION moments
      // earlier. The DEVICE_CMD handler is the single emotion path.

      auto new_sys = static_cast<state::SystemState>(system_state);
      if (new_sys != state::SystemState::BOOTING) {
        sm.setSystemState(new_sys);
      }

      // Mirror the interaction state (C5 owns the voice turn). Every
      // C5 state must be applied — the old partial mapping left S3
      // stuck in PROCESSING when C5 timed out back to IDLE.
      auto new_inter = static_cast<state::InteractionState>(interaction);
      auto cur_inter = sm.getInteractionState();
      if (new_inter == cur_inter)
        return;

      switch (new_inter) {
      case state::InteractionState::LISTENING:
        // Open our microphone. Use src=UNKNOWN (NOT BUTTON/WAKEWORD)
        // so AppController does NOT echo a START back to C5.
        if (cur_inter == state::InteractionState::SPEAKING && audio_raw) {
          audio_raw->stopSpeaking(true); // barge-in
        }
        sm.setInteractionState(state::InteractionState::LISTENING,
                               state::InputSource::UNKNOWN);
        break;

      case state::InteractionState::PROCESSING:
        if (cur_inter != state::InteractionState::SPEAKING) {
          sm.setInteractionState(new_inter, state::InputSource::UNKNOWN);
        }
        break;

      case state::InteractionState::SPEAKING:
        sm.setInteractionState(new_inter, state::InputSource::UNKNOWN);
        break;

      case state::InteractionState::IDLE:
      case state::InteractionState::TRIGGERED:
      case state::InteractionState::CANCELLING:
      default:
        // Treat TRIGGERED/CANCELLING like the IDLE edge: stop audio,
        // return to rest. Covers button-release, server audio_stop,
        // and C5 watchdog timeouts (incl. from PROCESSING).
        sm.setInteractionState(state::InteractionState::IDLE,
                               state::InputSource::UNKNOWN);
        break;
      }
    });

    DisplayManager *disp_raw = display_mgr.get();
    // The camera task flags its on-device capture indicator on this display.
    s_camera_disp = disp_raw;
    uart_bridge->onControlCmd([disp_raw, audio_raw](uart_proto::ControlCmd cmd,
                                                    const uint8_t *data,
                                                    size_t len) {
      switch (cmd) {
      case uart_proto::ControlCmd::SET_VOLUME:
        if (len >= 1 && audio_raw) {
          audio_raw->setVolume(data[0]);
          ESP_LOGI("UartCtrl", "SET_VOLUME %d from C5", data[0]);
        }
        break;
      case uart_proto::ControlCmd::SET_BRIGHTNESS:
        if (len >= 1 && disp_raw) {
          disp_raw->setBrightness(data[0]);
          ESP_LOGI("UartCtrl", "SET_BRIGHTNESS %d from C5", data[0]);
        }
        break;
      default:
        ESP_LOGW("UartCtrl", "Unknown control cmd 0x%02X from C5", (int)cmd);
        break;
      }
    });
    uart_bridge->onDeviceCmd([uart_raw, disp_raw,
                              audio_raw](uart_proto::ControlCmd cmd,
                                         const uint8_t *data, size_t len) {
      if (cmd == uart_proto::ControlCmd::SET_EMOTION && len > 0) {
        // Emotion key string from the server (relayed by C5) — the ONE
        // emotion path into the S3. Render the matching category;
        // SceneManager picks a random variant in it.
        char key[32];
        size_t n = (len < sizeof(key) - 1) ? len : sizeof(key) - 1;
        memcpy(key, data, n);
        key[n] = '\0';
        ESP_LOGI("UartDev", "SET_EMOTION: %s", key);

        // Bookkeeping first (may render a generic face via the
        // DisplayManager subscription), exact key render last so the
        // full-fidelity scene always wins.
        static const struct {
          const char *k;
          state::EmotionState v;
        } kMap[] = {
            {"neutral", state::EmotionState::NEUTRAL},
            {"happy", state::EmotionState::HAPPY},
            {"sad", state::EmotionState::SAD},
            {"angry", state::EmotionState::ANGRY},
            {"confused", state::EmotionState::CONFUSED},
            {"excited", state::EmotionState::EXCITED},
            {"calm", state::EmotionState::CALM},
            {"thinking", state::EmotionState::THINKING},
            {"disgusted", state::EmotionState::DISGUSTED},
            {"nervous", state::EmotionState::NERVOUS},
            {"embarrassed", state::EmotionState::EMBARRASSED},
            {"curious", state::EmotionState::CURIOUS},
            {"annoyed", state::EmotionState::ANNOYED},
            {"cool", state::EmotionState::COOL},
            {"suspicious", state::EmotionState::SUSPICIOUS},
            {"determined", state::EmotionState::DETERMINED},
        };
        auto emo = state::EmotionState::NEUTRAL;
        for (const auto &m : kMap) {
          if (strcmp(key, m.k) == 0) {
            emo = m.v;
            break;
          }
        }
        StateManager::instance().setEmotionState(emo);

        SceneManager::instance().showEmotion(key);
      } else if (cmd == uart_proto::ControlCmd::SET_SCENE && len > 0) {
        // Scene key from the server (relayed by C5). Render the matching
        // scene category; an unknown key logs a warning in SceneManager
        // instead of vanishing silently (R-DA-RBT-006).
        char key[32];
        size_t n = (len < sizeof(key) - 1) ? len : sizeof(key) - 1;
        memcpy(key, data, n);
        key[n] = '\0';
        ESP_LOGI("UartDev", "SET_SCENE: %s", key);
        SceneManager::instance().showScene(key);
      } else if (cmd == uart_proto::ControlCmd::BLE_PIN && len > 0 && len < 8) {
        auto &sd = SceneManager::instance().getSceneData();
        memcpy(sd.ble_pin, data, len);
        sd.ble_pin[len] = '\0';
        ESP_LOGI("UartDev", "BLE PIN: %s", sd.ble_pin);
      } else if (cmd == uart_proto::ControlCmd::CAMERA_CAPTURE) {
        // Capture + stream happen on the dedicated camera task (S9-01):
        // just notify it so this rx callback returns immediately and keeps
        // draining state/control frames. No-op without LUNI_ENABLE_CAMERA.
        if (s_camera_task)
          xTaskNotifyGive(s_camera_task);
      } else if (cmd == uart_proto::ControlCmd::SET_BATTERY && len >= 1) {
        // Battery report from the C5 (which owns the ADC now). Payload:
        // [percent (255 = hide)][power_state]. Drive the status-bar icon +
        // the charging/critical state.
        if (disp_raw)
          disp_raw->setBatteryPercent(data[0]);
        if (len >= 2) {
          StateManager::instance().setPowerState((state::PowerState)data[1]);
        }
      } else if (cmd == uart_proto::ControlCmd::SET_MIC_MUTE && len >= 1) {
        // Privacy mute toggled by the C5 button (the mic lives here on the
        // S3). Cut the I2S input + reflect it on the status-bar mic icon.
        bool muted = data[0] != 0;
        if (audio_raw)
          audio_raw->setMicMuted(muted);
        if (disp_raw)
          disp_raw->setMicMuted(muted);
        ESP_LOGI("UartDev", "Mic %s (privacy mute)", muted ? "MUTED" : "live");
      }
      // MOTION_CMD is no longer handled here — servos moved to the C5, which
      // drives them directly from the WS MOTION message (no UART hop).
    });

    // Sync data (time/weather/lunar/location) the C5 relayed from the
    // server. Decode the binary SYNC_DATA payload into a SyncDataPacket and
    // feed the DisplayManager (status-bar clock, weather/moon scenes). This
    // was dead before: sync_cb_ was never registered, so the channel was cut
    // even after the C5 relay was wired (S8-03/S9-05). Layout matches the C5
    // DataSyncManager::relayToS3 frame:
    //   [temp:int8][humidity][condition_enum][aqi]
    //   [unix_time:4 LE][utc_offset:int8][lunar_day][lunar_month]
    //   [city: null-terminated]
    uart_bridge->onSyncData([disp_raw](const uint8_t *data, size_t len) {
      if (!disp_raw || len < 11)
        return;
      state::SyncDataPacket pkt{};
      pkt.temperature = (int8_t)data[0];
      pkt.humidity = data[1];
      pkt.condition = data[2];
      pkt.aqi = data[3];
      pkt.unix_time = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                      ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
      pkt.utc_offset = (int8_t)data[8];
      pkt.lunar_day = data[9];
      pkt.lunar_month = data[10];
      size_t city_len = len - 11; // last byte is the C5's '\0'
      if (city_len > sizeof(pkt.city) - 1)
        city_len = sizeof(pkt.city) - 1;
      if (city_len > 0)
        memcpy(pkt.city, &data[11], city_len);
      pkt.city[city_len] = '\0';
      disp_raw->handleSyncData(pkt);
    });

    // OTA progress from C5 → state + the OTA screen (progress bar,
    // verifying/flashing/reboot/fail panels).
    uart_bridge->onOtaStatus([disp_raw](uint8_t st, uint8_t pct) {
      auto s = static_cast<state::OtaState>(st);
      StateManager::instance().setOtaState(s);
      if (disp_raw)
        disp_raw->handleOtaStatus(s, pct);
    });
  } else {
    ESP_LOGI(TAG, "UART bridge not wired, skipping UartBridge");
  }
  bootData.boot_check_results[BOOT_COMMS] = BOOT_OK;

  // =========================================================
  // 7. TOUCH INPUT (Button) — MOVED to ESP32-C5
  // =========================================================
  // The push-to-talk button now lives on the C5 (PinConfig::USER_BUTTON). S3
  // follows the LISTENING state pushed by C5 over UART (see onStatusUpdate).
  // A local button is only created if one is explicitly wired (BUTTON >= 0).
  std::unique_ptr<TouchInput> touch_input;
  if (PinConfig::BUTTON >= 0) {
    touch_input = std::make_unique<TouchInput>();
    TouchInput::Config touch_cfg{.pin = (gpio_num_t)PinConfig::BUTTON,
                                 .active_low = true,
                                 .long_press_ms = 1200};
    if (!touch_input->init(touch_cfg)) {
      ESP_LOGE(TAG, "TouchInput init failed");
      return false;
    }
    touch_input->onEvent([&app](TouchInput::Event e) {
      if (e == TouchInput::Event::PRESS)
        app.postEvent(event::AppEvent::USER_BUTTON);
      if (e == TouchInput::Event::RELEASE)
        app.postEvent(event::AppEvent::RELEASE_BUTTON);
    });
  } else {
    ESP_LOGI(TAG, "Button on C5; S3 follows LISTENING via UART status");
  }

  // =========================================================
  // 8. ATTACH MODULES -> APP CONTROLLER
  // =========================================================
  app.attachModules(std::move(display_mgr), std::move(power_mgr),
                    std::move(audio_mgr), std::move(spi_bridge),
                    std::move(uart_bridge), std::move(touch_input));

  ESP_LOGI(TAG, "DeviceProfile setup OK (ESP32-S3)");
  return true;
}
