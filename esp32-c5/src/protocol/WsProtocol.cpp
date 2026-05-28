#include "WsProtocol.hpp"
#include <cstring>
#include <cstdio>

namespace ws {

struct MsgTypeEntry {
    const char* str;
    MsgType type;
};

static constexpr MsgTypeEntry MSG_TYPE_MAP[] = {
    { "auth",              MsgType::AUTH },
    { "auth_result",       MsgType::AUTH_RESULT },
    { "device_info",       MsgType::DEVICE_INFO },
    { "state_update",      MsgType::STATE_UPDATE },
    { "heartbeat",         MsgType::HEARTBEAT },
    { "log",               MsgType::LOG },
    { "ota_progress",      MsgType::OTA_PROGRESS },
    { "error",             MsgType::ERROR_REPORT },
    { "set_volume",        MsgType::SET_VOLUME },
    { "set_brightness",    MsgType::SET_BRIGHTNESS },
    { "set_emotion",       MsgType::SET_EMOTION },
    { "set_scene",         MsgType::SET_SCENE },
    { "reboot",            MsgType::REBOOT },
    { "ota_available",     MsgType::OTA_AVAILABLE },
    { "sync_data",         MsgType::SYNC_DATA },
    { "tts_play",          MsgType::TTS_PLAY },
    { "audio_stop",        MsgType::AUDIO_STOP },
    { "config_update",     MsgType::CONFIG_UPDATE },
    { "interaction_msg",   MsgType::INTERACTION_MSG },
    { "ack",               MsgType::ACK },
};

static constexpr size_t MSG_TYPE_MAP_SIZE = sizeof(MSG_TYPE_MAP) / sizeof(MSG_TYPE_MAP[0]);

MsgType msgTypeFromString(const char* str)
{
    if (!str) return MsgType::UNKNOWN;
    for (size_t i = 0; i < MSG_TYPE_MAP_SIZE; ++i) {
        if (strcmp(str, MSG_TYPE_MAP[i].str) == 0) {
            return MSG_TYPE_MAP[i].type;
        }
    }
    return MsgType::UNKNOWN;
}

const char* msgTypeToString(MsgType type)
{
    for (size_t i = 0; i < MSG_TYPE_MAP_SIZE; ++i) {
        if (MSG_TYPE_MAP[i].type == type) {
            return MSG_TYPE_MAP[i].str;
        }
    }
    return "unknown";
}

// === Message builders ===

cJSON* createMessage(MsgType type, const char* id)
{
    cJSON* root = cJSON_CreateObject();
    if (!root) return nullptr;

    cJSON_AddStringToObject(root, "type", msgTypeToString(type));
    if (id) {
        cJSON_AddStringToObject(root, "id", id);
    }

    return root;
}

cJSON* createAuthMessage(const char* mac,
                         const char* fw_version, const char* model)
{
    cJSON* root = createMessage(MsgType::AUTH);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddStringToObject(payload, "mac", mac);
        cJSON_AddStringToObject(payload, "fw_version", fw_version);
        cJSON_AddStringToObject(payload, "model", model);
    }
    return root;
}

cJSON* createDeviceInfo(const char* mac, const char* fw_version,
                        const char* model, const char* name)
{
    cJSON* root = createMessage(MsgType::DEVICE_INFO);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddStringToObject(payload, "mac", mac);
        cJSON_AddStringToObject(payload, "fw_version", fw_version);
        cJSON_AddStringToObject(payload, "model", model);
        cJSON_AddStringToObject(payload, "name", name);
    }
    return root;
}

cJSON* createStateUpdate(const char* category, uint8_t old_val, uint8_t new_val)
{
    cJSON* root = createMessage(MsgType::STATE_UPDATE);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddStringToObject(payload, "category", category);
        cJSON_AddNumberToObject(payload, "old", old_val);
        cJSON_AddNumberToObject(payload, "new", new_val);
    }
    return root;
}

cJSON* createHeartbeat(uint32_t uptime, uint32_t free_heap, int8_t rssi)
{
    cJSON* root = createMessage(MsgType::HEARTBEAT);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddNumberToObject(payload, "uptime", uptime);
        cJSON_AddNumberToObject(payload, "free_heap", free_heap);
        cJSON_AddNumberToObject(payload, "rssi", rssi);
    }
    return root;
}

cJSON* createLog(const char* level, const char* tag, const char* message)
{
    cJSON* root = createMessage(MsgType::LOG);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddStringToObject(payload, "level", level);
        cJSON_AddStringToObject(payload, "tag", tag);
        cJSON_AddStringToObject(payload, "message", message);
    }
    return root;
}

cJSON* createOtaProgress(uint8_t percent, const char* phase)
{
    cJSON* root = createMessage(MsgType::OTA_PROGRESS);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddNumberToObject(payload, "percent", percent);
        cJSON_AddStringToObject(payload, "phase", phase);
    }
    return root;
}

cJSON* createErrorReport(uint16_t code, const char* message, const char* severity)
{
    cJSON* root = createMessage(MsgType::ERROR_REPORT);
    if (!root) return nullptr;

    cJSON* payload = cJSON_AddObjectToObject(root, "payload");
    if (payload) {
        cJSON_AddNumberToObject(payload, "code", code);
        cJSON_AddStringToObject(payload, "message", message);
        cJSON_AddStringToObject(payload, "severity", severity);
    }
    return root;
}

// === Parser ===

ParsedMessage parseMessage(const char* json, size_t len)
{
    ParsedMessage result{};
    result.type = MsgType::UNKNOWN;
    result.id[0] = '\0';
    result.payload = nullptr;
    result.root = nullptr;
    result.valid = false;

    if (!json || len == 0) return result;

    cJSON* root = cJSON_ParseWithLength(json, len);
    if (!root) return result;

    cJSON* type_item = cJSON_GetObjectItem(root, "type");
    if (!type_item || !cJSON_IsString(type_item)) {
        cJSON_Delete(root);
        return result;
    }

    result.type = msgTypeFromString(type_item->valuestring);

    cJSON* id_item = cJSON_GetObjectItem(root, "id");
    if (id_item && cJSON_IsString(id_item) && id_item->valuestring) {
        strncpy(result.id, id_item->valuestring, sizeof(result.id) - 1);
        result.id[sizeof(result.id) - 1] = '\0';
    }

    result.payload = cJSON_GetObjectItem(root, "payload");
    result.root = root;
    result.valid = true;
    return result;
}

char* serializeMessage(cJSON* msg)
{
    if (!msg) return nullptr;
    return cJSON_PrintUnformatted(msg);
}

} // namespace ws
