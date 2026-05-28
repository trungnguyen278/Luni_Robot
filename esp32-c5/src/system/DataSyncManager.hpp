#pragma once
#include <cstdint>
#include <cstddef>
#include <atomic>
#include "cJSON.h"

class UartBridge;

class DataSyncManager {
public:
    DataSyncManager() = default;
    ~DataSyncManager() = default;

    void init();

    void handleSyncData(cJSON* payload);

    void relayToS3(UartBridge& uart);

    bool hasValidData() const { return data_valid_; }
    uint32_t lastUpdateTime() const { return last_update_; }

    struct SyncData {
        int64_t unix_time;
        char    timezone[32];
        int8_t  utc_offset;

        int8_t  temperature;
        int8_t  feels_like;
        uint8_t humidity;
        char    condition[24];
        uint8_t aqi;

        struct {
            uint8_t day;
            uint8_t month;
            char    year[12];
        } lunar;

        char city[32];
    };

    const SyncData& getData() const { return data_; }

private:
    uint8_t conditionToEnum(const char* cond);

    SyncData data_{};
    bool     data_valid_ = false;
    uint32_t last_update_ = 0;
};
