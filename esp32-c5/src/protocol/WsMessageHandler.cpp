#include "WsMessageHandler.hpp"
#include "system/NetworkManager.hpp"
#include "system/StateManager.hpp"
#include "system/OtaManager.hpp"
#include "system/DataSyncManager.hpp"
#include "system/UartBridge.hpp"
#include "system/MotionManager.hpp"
#include "UartProtocol.hpp"
#include "Version.hpp"

#include "esp_log.h"
#include "nvs_flash.h"
#include <cstring>

static const char* TAG = "WsMsgHandler";

void WsMessageHandler::handleMessage(const ws::ParsedMessage& msg, NetworkManager* net)
{
    if (!msg.valid || !net) return;

    switch (msg.type) {
    case ws::MsgType::SET_VOLUME:      handleSetVolume(msg.payload, net); break;
    case ws::MsgType::SET_BRIGHTNESS:  handleSetBrightness(msg.payload, net); break;
    case ws::MsgType::SET_EMOTION:     handleSetEmotion(msg.payload, net); break;
    case ws::MsgType::SET_SCENE:       handleSetScene(msg.payload, net); break;
    case ws::MsgType::REBOOT:          handleReboot(msg.payload, net); break;
    case ws::MsgType::OTA_AVAILABLE:   handleOtaAvailable(msg.payload, net); break;
    case ws::MsgType::SYNC_DATA:       handleSyncData(msg.payload, net); break;
    case ws::MsgType::TTS_PLAY:        handleTtsPlay(msg.payload, net); break;
    case ws::MsgType::AUDIO_STOP:      handleAudioStop(msg.payload, net); break;
    case ws::MsgType::CONFIG_UPDATE:   handleConfigUpdate(msg.payload, net); break;
    case ws::MsgType::INTERACTION_MSG: handleInteractionMsg(msg.payload, net); break;
    case ws::MsgType::ACK:            handleAck(msg.payload, net); break;
    case ws::MsgType::MOTION:         handleMotion(msg.payload, net); break;
    case ws::MsgType::CAMERA_CAPTURE: handleCameraCapture(msg.payload, net); break;
    default:
        ESP_LOGW(TAG, "Unhandled message type: %s", ws::msgTypeToString(msg.type));
        break;
    }
}

void WsMessageHandler::handleSetVolume(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* val = cJSON_GetObjectItem(payload, "value");
    if (!val || !cJSON_IsNumber(val)) return;

    uint8_t volume = (uint8_t)val->valueint;
    ESP_LOGI(TAG, "SET_VOLUME: %d", volume);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "volume", volume);
        nvs_commit(h);
        nvs_close(h);
    }

    // Relay to S3 via UART
    uint8_t cmd_payload[] = { (uint8_t)uart_proto::ControlCmd::SET_VOLUME, volume };
    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::CONTROL_CMD,
                                         cmd_payload, sizeof(cmd_payload));
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, frame, len);
    }
}

void WsMessageHandler::handleSetBrightness(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* val = cJSON_GetObjectItem(payload, "value");
    if (!val || !cJSON_IsNumber(val)) return;

    uint8_t brightness = (uint8_t)val->valueint;
    ESP_LOGI(TAG, "SET_BRIGHTNESS: %d", brightness);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "brightness", brightness);
        nvs_commit(h);
        nvs_close(h);
    }

    uint8_t cmd_payload[] = { (uint8_t)uart_proto::ControlCmd::SET_BRIGHTNESS, brightness };
    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::CONTROL_CMD,
                                         cmd_payload, sizeof(cmd_payload));
    if (len > 0) {
        uart_write_bytes(UART_NUM_1, frame, len);
    }
}

void WsMessageHandler::handleSetEmotion(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* emotion = cJSON_GetObjectItem(payload, "emotion");
    if (!emotion || !cJSON_IsString(emotion)) return;

    ESP_LOGI(TAG, "SET_EMOTION: %s", emotion->valuestring);

    // Map emotion string to enum — covers every EmotionState value; the
    // remaining server emotion keys (45 total) fall back to NEUTRAL here but
    // keep full fidelity on the S3 via the forwarded key string below.
    static const struct { const char* key; state::EmotionState v; } kEmotionMap[] = {
        { "neutral",     state::EmotionState::NEUTRAL     },
        { "happy",       state::EmotionState::HAPPY       },
        { "sad",         state::EmotionState::SAD         },
        { "angry",       state::EmotionState::ANGRY       },
        { "confused",    state::EmotionState::CONFUSED    },
        { "excited",     state::EmotionState::EXCITED     },
        { "calm",        state::EmotionState::CALM        },
        { "thinking",    state::EmotionState::THINKING    },
        { "disgusted",   state::EmotionState::DISGUSTED   },
        { "nervous",     state::EmotionState::NERVOUS     },
        { "embarrassed", state::EmotionState::EMBARRASSED },
        { "curious",     state::EmotionState::CURIOUS     },
        { "annoyed",     state::EmotionState::ANNOYED     },
        { "cool",        state::EmotionState::COOL        },
        { "suspicious",  state::EmotionState::SUSPICIOUS  },
        { "determined",  state::EmotionState::DETERMINED  },
    };
    state::EmotionState emo = state::EmotionState::NEUTRAL;
    const char* e = emotion->valuestring;
    for (const auto& m : kEmotionMap) {
        if (strcmp(e, m.key) == 0) { emo = m.v; break; }
    }

    StateManager::instance().setEmotionState(emo);

    // Tell the app/web the real emotion (full 45-key string). setEmotionState
    // above fires a numeric state_update that NetworkManager now skips, so this
    // is the single emotion signal that reaches clients.
    if (net) net->sendEmotionUpdate(e);

    // Forward the FULL emotion key string to S3 so it can render any of the 45
    // emotion categories (the enum above only covers C5's own state tracking and
    // is limited to ~13 values). S3's DEVICE_CMD handler maps this key to
    // SceneManager::showEmotion(key), which plays a random variant in that list.
    size_t keylen = strlen(e);
    if (keylen > 0 && keylen < uart_proto::MAX_PAYLOAD - 1) {
        uint8_t cmd_payload[uart_proto::MAX_PAYLOAD];
        cmd_payload[0] = (uint8_t)uart_proto::ControlCmd::SET_EMOTION;
        memcpy(&cmd_payload[1], e, keylen);
        uint8_t frame[uart_proto::MAX_FRAME_SIZE];
        size_t flen = uart_proto::buildFrame(frame, uart_proto::MsgType::DEVICE_CMD,
                                             cmd_payload, (uint8_t)(keylen + 1));
        if (flen > 0) {
            uart_write_bytes(UART_NUM_1, frame, flen);
        }
    }
}

void WsMessageHandler::handleSetScene(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* scene = cJSON_GetObjectItem(payload, "scene");
    if (!scene || !cJSON_IsString(scene)) return;

    const char* s = scene->valuestring;
    ESP_LOGI(TAG, "SET_SCENE: %s", s);

    // Relay the scene key to the S3 via UART DEVICE_CMD (mirrors SET_EMOTION).
    // The S3's onDeviceCmd maps it to SceneManager::showScene(key); an unknown
    // key now logs a warning there instead of failing silently.
    size_t slen = strlen(s);
    if (slen > 0 && slen < uart_proto::MAX_PAYLOAD - 1) {
        uint8_t cmd_payload[uart_proto::MAX_PAYLOAD];
        cmd_payload[0] = (uint8_t)uart_proto::ControlCmd::SET_SCENE;
        memcpy(&cmd_payload[1], s, slen);
        uint8_t frame[uart_proto::MAX_FRAME_SIZE];
        size_t flen = uart_proto::buildFrame(frame, uart_proto::MsgType::DEVICE_CMD,
                                             cmd_payload, (uint8_t)(slen + 1));
        if (flen > 0) {
            uart_write_bytes(UART_NUM_1, frame, flen);
        }
    }
}

void WsMessageHandler::handleReboot(cJSON* payload, NetworkManager* net)
{
    ESP_LOGW(TAG, "REBOOT command received");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

void WsMessageHandler::handleOtaAvailable(cJSON* payload, NetworkManager* net)
{
    if (!payload || !net) return;

    cJSON* version = cJSON_GetObjectItem(payload, "version");
    cJSON* url = cJSON_GetObjectItem(payload, "url");
    cJSON* sha256 = cJSON_GetObjectItem(payload, "sha256");
    cJSON* size = cJSON_GetObjectItem(payload, "size");

    if (!version || !url || !sha256 || !size) {
        ESP_LOGW(TAG, "OTA_AVAILABLE: missing fields");
        return;
    }

    // Wrong-chip guard: this MCU only flashes images built for its own model.
    // The S3 has separate firmware and no direct OTA path yet — flashing an
    // S3 image onto the C5 partition would brick it. A missing `model` field
    // means a legacy server, which only ever ships C5 images, so allow it.
    cJSON* model = cJSON_GetObjectItem(payload, "model");
    if (cJSON_IsString(model) && strcmp(model->valuestring, app_meta::DLuniCE_MODEL) != 0) {
        ESP_LOGW(TAG, "OTA_AVAILABLE: model mismatch (got=%s, self=%s) — ignoring",
                 model->valuestring, app_meta::DLuniCE_MODEL);
        return;
    }

    ESP_LOGI(TAG, "OTA_AVAILABLE: v%s size=%d", version->valuestring, size->valueint);

    OtaManager* ota = net->getOtaManager();
    if (ota) {
        ota->startDownload(url->valuestring, sha256->valuestring, (size_t)size->valueint);
    }
}

void WsMessageHandler::handleSyncData(cJSON* payload, NetworkManager* net)
{
    if (!payload || !net) return;
    ESP_LOGI(TAG, "SYNC_DATA received");

    DataSyncManager* sync = net->getDataSync();
    if (sync) {
        sync->handleSyncData(payload);
        // Relay the parsed time/weather/lunar/location on to the S3. Without
        // this the sync stopped here at the C5 and the S3's clock/weather/moon
        // scenes never got real data (S8-03/S9-05).
        UartBridge* uart = net->getUartBridge();
        if (uart) sync->relayToS3(*uart);
    }
}

void WsMessageHandler::handleTtsPlay(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* text = cJSON_GetObjectItem(payload, "text");
    if (!text || !cJSON_IsString(text)) return;

    ESP_LOGI(TAG, "TTS_PLAY: %.32s...", text->valuestring);
    // Relay to S3 via UART DEVICE_CMD
}

void WsMessageHandler::handleAudioStop(cJSON* payload, NetworkManager* net)
{
    if (!net) return;
    ESP_LOGI(TAG, "AUDIO_STOP");
    net->endSpeakingSession();
    StateManager::instance().setInteractionState(
        state::InteractionState::IDLE, state::InputSource::SERVER_COMMAND);
}

void WsMessageHandler::handleConfigUpdate(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* key = cJSON_GetObjectItem(payload, "key");
    cJSON* value = cJSON_GetObjectItem(payload, "value");
    if (!key || !value || !cJSON_IsString(key)) return;

    const char* k = key->valuestring;

    // Allowlist of keys the server may write into the "storage" NVS namespace.
    // That namespace ALSO holds credentials (ssid/pass/ws_url/user_id/
    // device_token/admin_secret — see AppController commit). Without this gate a
    // compromised or MITM server could send config_update{key:"pass"} /
    // {"device_token"} / {"ws_url"} and silently rewrite credentials to hijack
    // or brick the robot. Restrict writes to non-credential, user-tunable keys.
    static const char* const kAllowedKeys[] = {
        "volume", "brightness", "log_level",
    };
    bool allowed = false;
    for (const char* ak : kAllowedKeys) {
        if (strcmp(k, ak) == 0) { allowed = true; break; }
    }
    if (!allowed) {
        ESP_LOGW(TAG, "CONFIG_UPDATE: key '%s' not allowed — ignoring", k);
        return;
    }

    ESP_LOGI(TAG, "CONFIG_UPDATE: %s", k);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        if (cJSON_IsString(value)) {
            nvs_set_str(h, k, value->valuestring);
        } else if (cJSON_IsNumber(value)) {
            nvs_set_u8(h, k, (uint8_t)value->valueint);
        }
        nvs_commit(h);
        nvs_close(h);
    }
}

void WsMessageHandler::handleInteractionMsg(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;
    cJSON* text = cJSON_GetObjectItem(payload, "text");
    if (!text || !cJSON_IsString(text)) return;

    ESP_LOGI(TAG, "INTERACTION_MSG: %.32s...", text->valuestring);
    // Relay to S3 for TTS playback
}

void WsMessageHandler::handleAck(cJSON* payload, NetworkManager* net)
{
    // ACK for tracked messages — currently no-op
}

void WsMessageHandler::handleMotion(cJSON* payload, NetworkManager* net)
{
    if (!payload) return;

    // "action": string (home/stop/walk_fwd/walk_back/turn_left/turn_right/
    //            gesture/set_joint) OR numeric MotionAction; optional p0, p1.
    uart_proto::MotionAction action = uart_proto::MotionAction::HOME;
    cJSON* act = cJSON_GetObjectItem(payload, "action");
    if (cJSON_IsString(act)) {
        const char* a = act->valuestring;
        if      (!strcmp(a, "home"))       action = uart_proto::MotionAction::HOME;
        else if (!strcmp(a, "stop"))       action = uart_proto::MotionAction::STOP;
        else if (!strcmp(a, "walk_fwd"))   action = uart_proto::MotionAction::WALK_FWD;
        else if (!strcmp(a, "walk_back"))  action = uart_proto::MotionAction::WALK_BACK;
        else if (!strcmp(a, "turn_left"))  action = uart_proto::MotionAction::TURN_LEFT;
        else if (!strcmp(a, "turn_right")) action = uart_proto::MotionAction::TURN_RIGHT;
        else if (!strcmp(a, "gesture"))    action = uart_proto::MotionAction::GESTURE;
        else if (!strcmp(a, "set_joint"))  action = uart_proto::MotionAction::SET_JOINT;
    } else if (cJSON_IsNumber(act)) {
        action = (uart_proto::MotionAction)act->valueint;
    }

    cJSON* p0j = cJSON_GetObjectItem(payload, "p0");
    cJSON* p1j = cJSON_GetObjectItem(payload, "p1");
    uint8_t p0 = cJSON_IsNumber(p0j) ? (uint8_t)p0j->valueint : 0;
    uint8_t p1 = cJSON_IsNumber(p1j) ? (uint8_t)p1j->valueint : 0;

    ESP_LOGI(TAG, "MOTION action=%d p0=%d p1=%d", (int)action, p0, p1);

    // Servos live on the C5 now (I2C moved here from the S3). Drive them
    // directly — no UART round-trip to the S3. post() is non-blocking; the
    // MotionManager task does the I2C work.
    if (g_motion) {
        g_motion->post(action, p0, p1);
    } else {
        ESP_LOGW(TAG, "MOTION dropped: MotionManager not ready");
    }
}

void WsMessageHandler::handleCameraCapture(cJSON* payload, NetworkManager* net)
{
    ESP_LOGI(TAG, "CAMERA_CAPTURE request");
    uint8_t cmd_payload[] = { (uint8_t)uart_proto::ControlCmd::CAMERA_CAPTURE };
    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::DEVICE_CMD,
                                        cmd_payload, sizeof(cmd_payload));
    if (len > 0) uart_write_bytes(UART_NUM_1, frame, len);
}
