#include "DataSyncManager.hpp"
#include "system/UartBridge.hpp"
#include "UartProtocol.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include <cstring>

static const char* TAG = "DataSync";

void DataSyncManager::init()
{
    memset(&data_, 0, sizeof(data_));
    data_valid_ = false;
    last_update_ = 0;
}

void DataSyncManager::handleSyncData(cJSON* payload)
{
    if (!payload) return;

    cJSON* time_obj = cJSON_GetObjectItem(payload, "time");
    if (time_obj) {
        cJSON* unix_ts = cJSON_GetObjectItem(time_obj, "unix");
        cJSON* tz      = cJSON_GetObjectItem(time_obj, "timezone");
        cJSON* offset  = cJSON_GetObjectItem(time_obj, "utc_offset");

        if (unix_ts && cJSON_IsNumber(unix_ts))
            data_.unix_time = (int64_t)unix_ts->valuedouble;
        if (tz && cJSON_IsString(tz))
            strncpy(data_.timezone, tz->valuestring, sizeof(data_.timezone) - 1);
        if (offset && cJSON_IsNumber(offset))
            data_.utc_offset = (int8_t)offset->valueint;
    }

    cJSON* weather = cJSON_GetObjectItem(payload, "weather");
    if (weather) {
        cJSON* temp      = cJSON_GetObjectItem(weather, "temperature");
        cJSON* feels     = cJSON_GetObjectItem(weather, "feels_like");
        cJSON* hum       = cJSON_GetObjectItem(weather, "humidity");
        cJSON* cond      = cJSON_GetObjectItem(weather, "condition");
        cJSON* aqi_val   = cJSON_GetObjectItem(weather, "aqi");

        if (temp && cJSON_IsNumber(temp))
            data_.temperature = (int8_t)temp->valueint;
        if (feels && cJSON_IsNumber(feels))
            data_.feels_like = (int8_t)feels->valueint;
        if (hum && cJSON_IsNumber(hum))
            data_.humidity = (uint8_t)hum->valueint;
        if (cond && cJSON_IsString(cond))
            strncpy(data_.condition, cond->valuestring, sizeof(data_.condition) - 1);
        if (aqi_val && cJSON_IsNumber(aqi_val))
            data_.aqi = (uint8_t)aqi_val->valueint;
    }

    cJSON* lunar = cJSON_GetObjectItem(payload, "lunar");
    if (lunar) {
        cJSON* day   = cJSON_GetObjectItem(lunar, "day");
        cJSON* month = cJSON_GetObjectItem(lunar, "month");
        cJSON* year  = cJSON_GetObjectItem(lunar, "year");

        if (day && cJSON_IsNumber(day))
            data_.lunar.day = (uint8_t)day->valueint;
        if (month && cJSON_IsNumber(month))
            data_.lunar.month = (uint8_t)month->valueint;
        if (year && cJSON_IsString(year))
            strncpy(data_.lunar.year, year->valuestring, sizeof(data_.lunar.year) - 1);
    }

    cJSON* location = cJSON_GetObjectItem(payload, "location");
    if (location) {
        cJSON* city = cJSON_GetObjectItem(location, "city");
        if (city && cJSON_IsString(city))
            strncpy(data_.city, city->valuestring, sizeof(data_.city) - 1);
    }

    data_valid_ = true;
    last_update_ = (uint32_t)(esp_timer_get_time() / 1000000);

    ESP_LOGI(TAG, "Sync: temp=%d hum=%d%% cond=%s city=%s",
             data_.temperature, data_.humidity, data_.condition, data_.city);
}

uint8_t DataSyncManager::conditionToEnum(const char* cond)
{
    if (!cond) return 0;
    if (strcmp(cond, "clear") == 0)          return 1;
    if (strcmp(cond, "partly_cloudy") == 0)  return 2;
    if (strcmp(cond, "cloudy") == 0)         return 3;
    if (strcmp(cond, "rain") == 0)           return 4;
    if (strcmp(cond, "heavy_rain") == 0)     return 5;
    if (strcmp(cond, "thunderstorm") == 0)   return 6;
    if (strcmp(cond, "snow") == 0)           return 7;
    if (strcmp(cond, "fog") == 0)            return 8;
    if (strcmp(cond, "windy") == 0)          return 9;
    if (strcmp(cond, "haze") == 0)           return 10;
    return 0;
}

void DataSyncManager::relayToS3(UartBridge& uart)
{
    if (!data_valid_) return;

    // Binary payload: [temp][humidity][condition_enum][aqi]
    //                 [unix_time:4 LE][utc_offset][lunar_day][lunar_month]
    //                 [city: null-terminated]
    uint8_t buf[64];
    size_t pos = 0;

    buf[pos++] = (uint8_t)data_.temperature;
    buf[pos++] = data_.humidity;
    buf[pos++] = conditionToEnum(data_.condition);
    buf[pos++] = data_.aqi;

    uint32_t ts32 = (uint32_t)(data_.unix_time & 0xFFFFFFFF);
    buf[pos++] = (uint8_t)(ts32 & 0xFF);
    buf[pos++] = (uint8_t)((ts32 >> 8) & 0xFF);
    buf[pos++] = (uint8_t)((ts32 >> 16) & 0xFF);
    buf[pos++] = (uint8_t)((ts32 >> 24) & 0xFF);

    buf[pos++] = (uint8_t)data_.utc_offset;
    buf[pos++] = data_.lunar.day;
    buf[pos++] = data_.lunar.month;

    size_t city_len = strlen(data_.city);
    if (city_len > sizeof(buf) - pos - 1) {
        city_len = sizeof(buf) - pos - 1;
    }
    memcpy(&buf[pos], data_.city, city_len);
    pos += city_len;
    buf[pos++] = '\0';

    // Send through the bridge's one framing+write path (no hardcoded UART_NUM_1,
    // no duplicate buildFrame). sendSyncData wraps this in a SYNC_DATA frame.
    uart.sendSyncData(buf, pos);
}
