#include "WsMessageHandler.hpp"
#include "system/NetworkManager.hpp"
#include "system/StateManager.hpp"
#include "system/OtaManager.hpp"
#include "system/DataSyncManager.hpp"
#include "system/UartBridge.hpp"
#include "UartProtocol.hpp"

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

    // Map emotion string to enum
    state::EmotionState emo = state::EmotionState::NEUTRAL;
    const char* e = emotion->valuestring;
    if (strcmp(e, "happy") == 0)       emo = state::EmotionState::HAPPY;
    else if (strcmp(e, "sad") == 0)    emo = state::EmotionState::SAD;
    else if (strcmp(e, "angry") == 0)  emo = state::EmotionState::ANGRY;
    else if (strcmp(e, "confused") == 0) emo = state::EmotionState::CONFUSED;
    else if (strcmp(e, "excited") == 0)  emo = state::EmotionState::EXCITED;
    else if (strcmp(e, "calm") == 0)     emo = state::EmotionState::CALM;
    else if (strcmp(e, "thinking") == 0) emo = state::EmotionState::THINKING;
    else if (strcmp(e, "disgusted") == 0) emo = state::EmotionState::DISGUSTED;
    else if (strcmp(e, "nervous") == 0)   emo = state::EmotionState::NERVOUS;
    else if (strcmp(e, "curious") == 0)   emo = state::EmotionState::CURIOUS;
    else if (strcmp(e, "annoyed") == 0)   emo = state::EmotionState::ANNOYED;
    else if (strcmp(e, "cool") == 0)      emo = state::EmotionState::COOL;

    StateManager::instance().setEmotionState(emo);

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
    ESP_LOGI(TAG, "SET_SCENE");
    // Relay scene command to S3 via UART DEVICE_CMD
    // S3 SceneManager will handle it
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

    ESP_LOGI(TAG, "CONFIG_UPDATE: %s", key->valuestring);

    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        if (cJSON_IsString(value)) {
            nvs_set_str(h, key->valuestring, value->valuestring);
        } else if (cJSON_IsNumber(value)) {
            nvs_set_u8(h, key->valuestring, (uint8_t)value->valueint);
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

    uint8_t cmd_payload[] = { (uint8_t)uart_proto::ControlCmd::MOTION_CMD,
                              (uint8_t)action, p0, p1 };
    uint8_t frame[uart_proto::MAX_FRAME_SIZE];
    size_t len = uart_proto::buildFrame(frame, uart_proto::MsgType::DEVICE_CMD,
                                        cmd_payload, sizeof(cmd_payload));
    if (len > 0) uart_write_bytes(UART_NUM_1, frame, len);
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
