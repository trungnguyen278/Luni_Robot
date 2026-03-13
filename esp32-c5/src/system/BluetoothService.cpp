#include "BluetoothService.hpp"
#include "esp_log.h"
#include <cstring>
#include <algorithm>
#include <set>
#include "WifiService.hpp"

static const char *TAG = "BT_SVC";

BluetoothService *BluetoothService::s_instance = nullptr;

BluetoothService::BluetoothService()
{
    s_instance = this;
}

BluetoothService::~BluetoothService()
{
    stop();
}

bool BluetoothService::init(const std::string &adv_name, const std::vector<WifiInfo> &cached_networks, const ConfigData *current_config)
{
    static bool s_bt_initialized = false;
    if (s_bt_initialized)
        return true;

    adv_name_ = adv_name;

    if (current_config)
        temp_cfg_ = *current_config;

    // ESP32-C5 is BLE-only, no classic BT memory to release
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK)
        return false;
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK)
        return false;
    if (esp_bluedroid_init() != ESP_OK)
        return false;
    if (esp_bluedroid_enable() != ESP_OK)
        return false;

    esp_ble_gatts_register_callback(BluetoothService::gattsEventHandler);
    esp_ble_gap_register_callback(BluetoothService::gapEventHandler);
    esp_ble_gatts_app_register(0);

    device_id_str_ = getDeviceEfuseID();

    // Prepare Wi-Fi list sorted by RSSI and deduplicated
    {
        wifi_networks_ = cached_networks;
        std::sort(wifi_networks_.begin(), wifi_networks_.end(),
                  [](const WifiInfo &a, const WifiInfo &b) { return a.rssi > b.rssi; });

        std::vector<WifiInfo> cleaned;
        std::set<std::string> seen;
        for (auto &net : wifi_networks_)
        {
            if (!net.ssid.empty() && seen.find(net.ssid) == seen.end())
            {
                cleaned.push_back(net);
                seen.insert(net.ssid);
            }
        }
        wifi_networks_ = cleaned;
        wifi_read_index_ = 0;

        ESP_LOGI(TAG, "WiFi list prepared: %d networks", (int)wifi_networks_.size());
    }
    s_bt_initialized = true;
    return true;
}

bool BluetoothService::start()
{
    if (started_)
        return true;

    adv_params_ = {};
    adv_params_.adv_int_min = 0x20;
    adv_params_.adv_int_max = 0x40;
    adv_params_.adv_type = ADV_TYPE_IND;
    adv_params_.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
    adv_params_.channel_map = ADV_CHNL_ALL;
    adv_params_.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

    const char* ble_adv_name = "PTalk";
    esp_ble_gap_set_device_name(ble_adv_name);

    uint8_t adv_data[31];
    size_t adv_len = 0;

    // Service UUID 0xFF01 in advertising data
    adv_data[adv_len++] = 3;
    adv_data[adv_len++] = 0x03;   // Complete List of 16-bit UUIDs
    adv_data[adv_len++] = 0x01;   // 0xFF01 LSB
    adv_data[adv_len++] = 0xFF;   // 0xFF01 MSB

    // Scan response with device name
    uint8_t scan_data[31];
    size_t scan_len = 0;
    size_t name_len = strlen(ble_adv_name);

    scan_data[scan_len++] = name_len + 1;
    scan_data[scan_len++] = 0x09;          // Complete Local Name
    memcpy(&scan_data[scan_len], ble_adv_name, name_len);
    scan_len += name_len;

    esp_ble_gap_config_adv_data_raw(adv_data, adv_len);
    esp_ble_gap_config_scan_rsp_data_raw(scan_data, scan_len);

    esp_ble_gap_start_advertising(&adv_params_);

    started_ = true;
    ESP_LOGI(TAG, "BLE Advertising started: %s", ble_adv_name);
    return true;
}

void BluetoothService::stop()
{
    if (!started_)
        return;
    esp_ble_gap_stop_advertising();
    started_ = false;
}

void BluetoothService::gattsEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    if (!s_instance)
        return;

    switch (event)
    {
    case ESP_GATTS_REG_EVT:
    {
        esp_gatt_srvc_id_t service_id;
        service_id.is_primary = true;
        service_id.id.inst_id = 0x00;
        service_id.id.uuid.len = ESP_UUID_LEN_16;
        service_id.id.uuid.uuid.uuid16 = SVC_UUID_CONFIG;
        esp_ble_gatts_create_service(gatts_if, &service_id, 29);
        break;
    }

    case ESP_GATTS_CREATE_EVT:
    {
        s_instance->gatts_if_ = gatts_if;
        s_instance->service_handle_ = param->create.service_handle;
        esp_ble_gatts_start_service(s_instance->service_handle_);

        auto add_c = [&](uint16_t uuid, uint8_t prop)
        {
            esp_bt_uuid_t char_uuid;
            char_uuid.len = ESP_UUID_LEN_16;
            char_uuid.uuid.uuid16 = uuid;
            esp_ble_gatts_add_char(s_instance->service_handle_, &char_uuid,
                                   ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, prop, NULL, NULL);
        };

        add_c(CHR_UUID_DEVICE_NAME, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_VOLUME, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_BRIGHTNESS, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_WIFI_SSID, ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_WIFI_PASS, ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_APP_VERSION, ESP_GATT_CHAR_PROP_BIT_READ);
        add_c(CHR_UUID_BUILD_INFO, ESP_GATT_CHAR_PROP_BIT_READ);
        add_c(CHR_UUID_SAVE_CMD, ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_DEVICE_ID, ESP_GATT_CHAR_PROP_BIT_READ);
        add_c(CHR_UUID_WIFI_LIST, ESP_GATT_CHAR_PROP_BIT_READ);
        add_c(CHR_UUID_WS_URL, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_MQTT_URL, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_MQTT_USER, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        add_c(CHR_UUID_MQTT_PASS, ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE);
        break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
    {
        static int char_idx = 0;
        if (char_idx < 14)
            s_instance->char_handles[char_idx++] = param->add_char.attr_handle;
        break;
    }

    case ESP_GATTS_MTU_EVT:
    {
        s_instance->mtu_size_ = param->mtu.mtu;
        ESP_LOGI(TAG, "MTU exchanged: %d bytes", param->mtu.mtu);
        break;
    }

    case ESP_GATTS_CONNECT_EVT:
        s_instance->conn_id_ = param->connect.conn_id;
        s_instance->url_unlocked_ = false;
        ESP_LOGI(TAG, "BLE Connected: conn_id=%d", param->connect.conn_id);
        break;

    case ESP_GATTS_DISCONNECT_EVT:
        s_instance->url_unlocked_ = false;
        ESP_LOGI(TAG, "BLE Disconnected: conn_id=%d, reason=0x%x",
                 param->disconnect.conn_id, param->disconnect.reason);
        if (s_instance->started_)
        {
            esp_ble_gap_start_advertising(&s_instance->adv_params_);
            ESP_LOGI(TAG, "BLE Advertising restarted after disconnect");
        }
        break;

    case ESP_GATTS_WRITE_EVT:
        s_instance->handleWrite(param);
        break;

    case ESP_GATTS_READ_EVT:
        s_instance->handleRead(param, gatts_if);
        break;

    default:
        break;
    }
}

void BluetoothService::handleWrite(esp_ble_gatts_cb_param_t *param)
{
    uint16_t h = param->write.handle;
    uint8_t *v = param->write.value;
    uint16_t l = param->write.len;

    if (h == char_handles[0])
        temp_cfg_.device_name.assign((char *)v, l);
    else if (h == char_handles[1])
        temp_cfg_.volume = v[0];
    else if (h == char_handles[2])
        temp_cfg_.brightness = v[0];
    else if (h == char_handles[3])
        temp_cfg_.ssid.assign((char *)v, l);
    else if (h == char_handles[4])
        temp_cfg_.pass.assign((char *)v, l);
    else if (h == char_handles[10])
    {
        if (!url_unlocked_)
        {
            std::string token(reinterpret_cast<char *>(v), reinterpret_cast<char *>(v) + l);
            if (token == WS_URL_AUTH_TOKEN)
            {
                url_unlocked_ = true;
                ESP_LOGI(TAG, "Auth unlocked by token");
            }
        }
        else
        {
            temp_cfg_.ws_url.assign((char *)v, l);
            ESP_LOGI(TAG, "WS URL set (%d bytes)", (int)l);
        }
    }
    else if (h == char_handles[11])
    {
        if (!url_unlocked_)
        {
            std::string token(reinterpret_cast<char *>(v), reinterpret_cast<char *>(v) + l);
            if (token == WS_URL_AUTH_TOKEN) { url_unlocked_ = true; }
        }
        else
        {
            temp_cfg_.mqtt_url.assign((char *)v, l);
            ESP_LOGI(TAG, "MQTT URL set (%d bytes)", (int)l);
        }
    }
    else if (h == char_handles[12])
    {
        if (!url_unlocked_)
        {
            std::string token(reinterpret_cast<char *>(v), reinterpret_cast<char *>(v) + l);
            if (token == WS_URL_AUTH_TOKEN) { url_unlocked_ = true; }
        }
        else
        {
            temp_cfg_.mqtt_user.assign((char *)v, l);
        }
    }
    else if (h == char_handles[13])
    {
        if (!url_unlocked_)
        {
            std::string token(reinterpret_cast<char *>(v), reinterpret_cast<char *>(v) + l);
            if (token == WS_URL_AUTH_TOKEN) { url_unlocked_ = true; }
        }
        else
        {
            temp_cfg_.mqtt_pass.assign((char *)v, l);
        }
    }
    else if (h == char_handles[7])
    {
        if (l > 0 && v[0] == 0x01 && config_cb_)
            config_cb_(temp_cfg_);
    }

    if (param->write.need_rsp)
    {
        esp_ble_gatts_send_response(gatts_if_, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);
    }
}

void BluetoothService::handleRead(esp_ble_gatts_cb_param_t *param, esp_gatt_if_t gatts_if)
{
    esp_gatt_rsp_t rsp = {};
    rsp.attr_value.handle = param->read.handle;

    // DEVICE_NAME
    if (param->read.handle == char_handles[0])
    {
        rsp.attr_value.len = temp_cfg_.device_name.length();
        memcpy(rsp.attr_value.value, temp_cfg_.device_name.c_str(), rsp.attr_value.len);
    }
    // VOLUME
    else if (param->read.handle == char_handles[1])
    {
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = temp_cfg_.volume;
    }
    // BRIGHTNESS
    else if (param->read.handle == char_handles[2])
    {
        rsp.attr_value.len = 1;
        rsp.attr_value.value[0] = temp_cfg_.brightness;
    }
    // APP_VERSION
    else if (param->read.handle == char_handles[5])
    {
        rsp.attr_value.len = strlen(app_meta::APP_VERSION);
        memcpy(rsp.attr_value.value, app_meta::APP_VERSION, rsp.attr_value.len);
    }
    // BUILD_INFO
    else if (param->read.handle == char_handles[6])
    {
        std::string info = std::string(app_meta::DEVICE_MODEL) + " (" + app_meta::BUILD_DATE + ")";
        rsp.attr_value.len = info.length();
        memcpy(rsp.attr_value.value, info.c_str(), rsp.attr_value.len);
    }
    // DEVICE_ID
    else if (param->read.handle == char_handles[8])
    {
        rsp.attr_value.len = device_id_str_.length();
        memcpy(rsp.attr_value.value, device_id_str_.c_str(), rsp.attr_value.len);
    }
    // WIFI_LIST (streaming)
    else if (param->read.handle == char_handles[9])
    {
        if (wifi_read_index_ < wifi_networks_.size())
        {
            auto &net = wifi_networks_[wifi_read_index_];
            std::string response = net.ssid + ":" + std::to_string(net.rssi);

            size_t max_payload = mtu_size_ > 3 ? (mtu_size_ - 3) : 20;
            if (response.length() > max_payload)
                response = response.substr(0, max_payload);

            rsp.attr_value.len = response.length();
            memcpy(rsp.attr_value.value, response.c_str(), rsp.attr_value.len);
            wifi_read_index_++;
        }
        else
        {
            std::string end_msg = "END";
            rsp.attr_value.len = end_msg.length();
            memcpy(rsp.attr_value.value, end_msg.c_str(), rsp.attr_value.len);
            wifi_read_index_ = 0;
        }
    }
    // WS_URL
    else if (param->read.handle == char_handles[10])
    {
        if (!url_unlocked_)
        {
            static const char locked_msg[] = "LOCKED";
            rsp.attr_value.len = sizeof(locked_msg) - 1;
            memcpy(rsp.attr_value.value, locked_msg, rsp.attr_value.len);
        }
        else if (temp_cfg_.ws_url.empty())
        {
            static const char empty_msg[] = "EMPTY";
            rsp.attr_value.len = sizeof(empty_msg) - 1;
            memcpy(rsp.attr_value.value, empty_msg, rsp.attr_value.len);
        }
        else
        {
            rsp.attr_value.len = temp_cfg_.ws_url.length();
            memcpy(rsp.attr_value.value, temp_cfg_.ws_url.c_str(), rsp.attr_value.len);
        }
    }
    // MQTT_URL
    else if (param->read.handle == char_handles[11])
    {
        if (!url_unlocked_)
        {
            static const char locked_msg[] = "LOCKED";
            rsp.attr_value.len = sizeof(locked_msg) - 1;
            memcpy(rsp.attr_value.value, locked_msg, rsp.attr_value.len);
        }
        else if (temp_cfg_.mqtt_url.empty())
        {
            static const char empty_msg[] = "EMPTY";
            rsp.attr_value.len = sizeof(empty_msg) - 1;
            memcpy(rsp.attr_value.value, empty_msg, rsp.attr_value.len);
        }
        else
        {
            rsp.attr_value.len = temp_cfg_.mqtt_url.length();
            memcpy(rsp.attr_value.value, temp_cfg_.mqtt_url.c_str(), rsp.attr_value.len);
        }
    }
    // MQTT_USER
    else if (param->read.handle == char_handles[12])
    {
        if (!url_unlocked_)
        {
            static const char locked_msg[] = "LOCKED";
            rsp.attr_value.len = sizeof(locked_msg) - 1;
            memcpy(rsp.attr_value.value, locked_msg, rsp.attr_value.len);
        }
        else if (temp_cfg_.mqtt_user.empty())
        {
            static const char empty_msg[] = "EMPTY";
            rsp.attr_value.len = sizeof(empty_msg) - 1;
            memcpy(rsp.attr_value.value, empty_msg, rsp.attr_value.len);
        }
        else
        {
            rsp.attr_value.len = temp_cfg_.mqtt_user.length();
            memcpy(rsp.attr_value.value, temp_cfg_.mqtt_user.c_str(), rsp.attr_value.len);
        }
    }
    // MQTT_PASS
    else if (param->read.handle == char_handles[13])
    {
        if (!url_unlocked_)
        {
            static const char locked_msg[] = "LOCKED";
            rsp.attr_value.len = sizeof(locked_msg) - 1;
            memcpy(rsp.attr_value.value, locked_msg, rsp.attr_value.len);
        }
        else if (temp_cfg_.mqtt_pass.empty())
        {
            static const char empty_msg[] = "EMPTY";
            rsp.attr_value.len = sizeof(empty_msg) - 1;
            memcpy(rsp.attr_value.value, empty_msg, rsp.attr_value.len);
        }
        else
        {
            rsp.attr_value.len = temp_cfg_.mqtt_pass.length();
            memcpy(rsp.attr_value.value, temp_cfg_.mqtt_pass.c_str(), rsp.attr_value.len);
        }
    }

    esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id, ESP_GATT_OK, &rsp);
}

void BluetoothService::gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {}
