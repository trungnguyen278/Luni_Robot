#pragma once
#include "WsProtocol.hpp"

class NetworkManager;

class WsMessageHandler {
public:
    static void handleMessage(const ws::ParsedMessage& msg, NetworkManager* net);

private:
    static void handleSetVolume(cJSON* payload, NetworkManager* net);
    static void handleSetBrightness(cJSON* payload, NetworkManager* net);
    static void handleSetEmotion(cJSON* payload, NetworkManager* net);
    static void handleSetScene(cJSON* payload, NetworkManager* net);
    static void handleReboot(cJSON* payload, NetworkManager* net);
    static void handleOtaAvailable(cJSON* payload, NetworkManager* net);
    static void handleSyncData(cJSON* payload, NetworkManager* net);
    static void handleTtsPlay(cJSON* payload, NetworkManager* net);
    static void handleAudioStop(cJSON* payload, NetworkManager* net);
    static void handleConfigUpdate(cJSON* payload, NetworkManager* net);
    static void handleInteractionMsg(cJSON* payload, NetworkManager* net);
    static void handleAck(cJSON* payload, NetworkManager* net);
    static void handleMotion(cJSON* payload, NetworkManager* net);
    static void handleCameraCapture(cJSON* payload, NetworkManager* net);
};
