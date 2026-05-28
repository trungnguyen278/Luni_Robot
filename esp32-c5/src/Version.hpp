#pragma once
#include "esp_system.h"
#include "esp_mac.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace app_meta {
    static constexpr const char* APP_VERSION  = "2.0.0";
    static constexpr const char* DLuniCE_MODEL = "Luni-C5";
    static constexpr const char* BUILD_DATE   = __DATE__;
}

inline void getDLuniceMacBytes(uint8_t out_mac[6])
{
    esp_read_mac(out_mac, ESP_MAC_BT);
}

inline const char* getDLuniceEfuseID()
{
    static char id[13] = {0};
    if (id[0] == 0) {
        uint8_t mac[6];
        getDLuniceMacBytes(mac);
        snprintf(id, sizeof(id), "%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return id;
}
