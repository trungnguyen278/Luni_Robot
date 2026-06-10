#include "BluetoothService.hpp"
#include "esp_log.h"
#include "esp_random.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include <cstring>
#include <algorithm>
#include <set>
#include "WifiService.hpp"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_bt.h"

#include "mbedtls/md.h"

static const char* TAG = "BT_SVC";

BluetoothService* BluetoothService::s_instance = nullptr;

// Characteristic indices in GATT table
enum ChrIdx {
    IDX_SSID = 0,
    IDX_PASSWORD,
    IDX_WS_URL,
    IDX_COMMIT,
    IDX_USER_ID,
    IDX_DEV_TOKEN,
    IDX_DEVICE_INFO,
    IDX_DIAG_INFO,
    IDX_AUTH_UNLOCK,
    IDX_COMMAND,
    IDX_ADMIN_AUTH,
    IDX_LOG_LEVEL,
    IDX_ADMIN_SECRET,
    IDX_WIFI_SCAN,
    IDX_COUNT
};

static uint16_t chr_val_handles[IDX_COUNT] = {0};

static int gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg);

static ble_uuid16_t svc_uuid       = BLE_UUID16_INIT(0xFF01);
static ble_uuid16_t chr_uuid_ssid  = BLE_UUID16_INIT(0x0001);
static ble_uuid16_t chr_uuid_pass  = BLE_UUID16_INIT(0x0002);
static ble_uuid16_t chr_uuid_ws    = BLE_UUID16_INIT(0x0003);
static ble_uuid16_t chr_uuid_commit = BLE_UUID16_INIT(0x0004);
static ble_uuid16_t chr_uuid_uid   = BLE_UUID16_INIT(0x0005);
static ble_uuid16_t chr_uuid_token = BLE_UUID16_INIT(0x0008);
static ble_uuid16_t chr_uuid_info  = BLE_UUID16_INIT(0x0006);
static ble_uuid16_t chr_uuid_diag  = BLE_UUID16_INIT(0x0007);
static ble_uuid16_t chr_uuid_auth  = BLE_UUID16_INIT(0x0010);
static ble_uuid16_t chr_uuid_cmd   = BLE_UUID16_INIT(0x0011);
static ble_uuid16_t chr_uuid_admin = BLE_UUID16_INIT(0x0012);
static ble_uuid16_t chr_uuid_log   = BLE_UUID16_INIT(0x0013);
static ble_uuid16_t chr_uuid_sec   = BLE_UUID16_INIT(0x0014);
static ble_uuid16_t chr_uuid_wifi  = BLE_UUID16_INIT(0x0009);

#define RW_FLAGS  (BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE)
#define RWN_FLAGS (BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY)
#define R_FLAGS   BLE_GATT_CHR_F_READ
#define W_FLAGS   BLE_GATT_CHR_F_WRITE
#define WN_FLAGS  (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY)

static const struct ble_gatt_chr_def gatt_chars[] = {
    { .uuid = &chr_uuid_ssid.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_SSID,         .flags = RW_FLAGS, .val_handle = &chr_val_handles[IDX_SSID] },
    { .uuid = &chr_uuid_pass.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_PASSWORD,      .flags = W_FLAGS,  .val_handle = &chr_val_handles[IDX_PASSWORD] },
    { .uuid = &chr_uuid_ws.u,    .access_cb = gatt_chr_access, .arg = (void*)IDX_WS_URL,        .flags = RW_FLAGS, .val_handle = &chr_val_handles[IDX_WS_URL] },
    { .uuid = &chr_uuid_commit.u,.access_cb = gatt_chr_access, .arg = (void*)IDX_COMMIT,         .flags = WN_FLAGS, .val_handle = &chr_val_handles[IDX_COMMIT] },
    { .uuid = &chr_uuid_uid.u,   .access_cb = gatt_chr_access, .arg = (void*)IDX_USER_ID,       .flags = RW_FLAGS, .val_handle = &chr_val_handles[IDX_USER_ID] },
    { .uuid = &chr_uuid_token.u,.access_cb = gatt_chr_access, .arg = (void*)IDX_DEV_TOKEN,     .flags = W_FLAGS,  .val_handle = &chr_val_handles[IDX_DEV_TOKEN] },
    { .uuid = &chr_uuid_info.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_DEVICE_INFO,   .flags = R_FLAGS,  .val_handle = &chr_val_handles[IDX_DEVICE_INFO] },
    { .uuid = &chr_uuid_diag.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_DIAG_INFO,     .flags = R_FLAGS,  .val_handle = &chr_val_handles[IDX_DIAG_INFO] },
    { .uuid = &chr_uuid_auth.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_AUTH_UNLOCK,   .flags = WN_FLAGS, .val_handle = &chr_val_handles[IDX_AUTH_UNLOCK] },
    { .uuid = &chr_uuid_cmd.u,   .access_cb = gatt_chr_access, .arg = (void*)IDX_COMMAND,       .flags = WN_FLAGS, .val_handle = &chr_val_handles[IDX_COMMAND] },
    { .uuid = &chr_uuid_admin.u, .access_cb = gatt_chr_access, .arg = (void*)IDX_ADMIN_AUTH,    .flags = WN_FLAGS, .val_handle = &chr_val_handles[IDX_ADMIN_AUTH] },
    { .uuid = &chr_uuid_log.u,   .access_cb = gatt_chr_access, .arg = (void*)IDX_LOG_LEVEL,     .flags = RW_FLAGS, .val_handle = &chr_val_handles[IDX_LOG_LEVEL] },
    { .uuid = &chr_uuid_sec.u,   .access_cb = gatt_chr_access, .arg = (void*)IDX_ADMIN_SECRET,  .flags = W_FLAGS,  .val_handle = &chr_val_handles[IDX_ADMIN_SECRET] },
    { .uuid = &chr_uuid_wifi.u,  .access_cb = gatt_chr_access, .arg = (void*)IDX_WIFI_SCAN,     .flags = R_FLAGS,  .val_handle = &chr_val_handles[IDX_WIFI_SCAN] },
    { 0 }
};

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = gatt_chars,
    },
    { 0 }
};

// === GAP event handler ===

static int ble_gap_event(struct ble_gap_event* event, void* arg)
{
    auto* self = BluetoothService::s_instance;
    if (!self) return 0;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "BLE Connected: handle=%d status=%d",
                 event->connect.conn_handle, event->connect.status);
        if (event->connect.status == 0) {
            self->conn_handle_ = event->connect.conn_handle;
            self->connected_ = true;
            self->started_ = false;
            self->access_level_ = BluetoothService::AccessLevel::LEVEL_0;
            if (self->conn_cb_) self->conn_cb_(true);
        } else {
            self->started_ = false;
            self->start();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "BLE Disconnected: reason=0x%x", event->disconnect.reason);
        self->connected_ = false;
        self->access_level_ = BluetoothService::AccessLevel::LEVEL_0;
        if (self->conn_cb_) self->conn_cb_(false);
        self->start();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
        break;

    default:
        break;
    }
    return 0;
}

// === GATT access callback ===

static int gatt_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt* ctxt, void* arg)
{
    auto* self = BluetoothService::s_instance;
    if (!self) return BLE_ATT_ERR_UNLIKELY;

    int idx = (int)(intptr_t)arg;
    auto level = self->access_level_.load();

    // === WRITE ===
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t buf[256];
        if (len > sizeof(buf)) len = sizeof(buf);
        os_mbuf_copydata(ctxt->om, 0, len, buf);

        switch (idx) {
        case IDX_AUTH_UNLOCK: {
            // PIN check → Level 1
            if (len == 6 && memcmp(buf, self->pairing_pin_, 6) == 0) {
                self->access_level_ = BluetoothService::AccessLevel::LEVEL_1;
                ESP_LOGI(TAG, "Auth → Level 1");
                self->notifyCommandResult(BluetoothService::CMD_OK);
            } else {
                ESP_LOGW(TAG, "PIN mismatch");
                self->notifyCommandResult(BluetoothService::CMD_FAIL);
            }
            return 0;
        }

        case IDX_ADMIN_AUTH: {
            if (level < BluetoothService::AccessLevel::LEVEL_1) {
                self->notifyCommandResult(BluetoothService::CMD_UNAUTHORIZED);
                return 0;
            }
            if (self->verifyAdminToken(buf, len)) {
                self->access_level_ = BluetoothService::AccessLevel::LEVEL_2;
                ESP_LOGI(TAG, "Auth → Level 2");
                self->notifyCommandResult(BluetoothService::CMD_OK);
            } else {
                ESP_LOGW(TAG, "Admin auth failed");
                self->notifyCommandResult(BluetoothService::CMD_FAIL);
            }
            return 0;
        }

        case IDX_COMMAND: {
            if (len < 1) return 0;
            auto cmd = (BluetoothService::BleCommand)buf[0];

            // Level check
            uint8_t required = (cmd == BluetoothService::BleCommand::RESTART)
                ? 1 : 2;
            if ((uint8_t)level < required) {
                self->notifyCommandResult(BluetoothService::CMD_UNAUTHORIZED);
                return 0;
            }

            self->notifyCommandResult(BluetoothService::CMD_OK);
            vTaskDelay(pdMS_TO_TICKS(500));
            self->executeCommand(cmd);
            return 0;
        }

        // Level 1+ writes
        case IDX_SSID:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.ssid.assign((char*)buf, len);
            break;

        case IDX_PASSWORD:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.pass.assign((char*)buf, len);
            break;

        case IDX_WS_URL:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.ws_url.assign((char*)buf, len);
            break;

        case IDX_COMMIT: {
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            if (self->temp_cfg_.ssid.empty() || self->temp_cfg_.pass.empty()) {
                ESP_LOGW(TAG, "COMMIT rejected: SSID or password missing");
                self->notifyCommitResult(0x01);
                return 0;
            }
            ESP_LOGI(TAG, "COMMIT: provisioning ssid='%s'", self->temp_cfg_.ssid.c_str());
            self->notifyCommitResult(0x00);
            if (self->config_cb_) {
                self->config_cb_(self->temp_cfg_);
            }
            return 0;
        }

        case IDX_USER_ID:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.user_id.assign((char*)buf, len);
            break;

        case IDX_DEV_TOKEN:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.device_token.assign((char*)buf, len);
            break;

        case IDX_ADMIN_SECRET:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            self->temp_cfg_.admin_secret.assign((char*)buf, len);
            break;

        case IDX_LOG_LEVEL:
            if (level < BluetoothService::AccessLevel::LEVEL_2)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            if (len >= 1) {
                nvs_handle_t h;
                if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
                    nvs_set_u8(h, "log_level", buf[0]);
                    nvs_commit(h);
                    nvs_close(h);
                }
            }
            break;

        default:
            break;
        }

        return 0;
    }

    // === READ ===
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        std::string response;

        switch (idx) {
        case IDX_DEVICE_INFO: {
            // Level 0 — public
            char info[192];
            snprintf(info, sizeof(info),
                "{\"mac\":\"%s\",\"model\":\"%s\",\"fw_version\":\"%s\",\"name\":\"%s\"}",
                self->device_id_str_.c_str(),
                app_meta::DLuniCE_MODEL,
                app_meta::APP_VERSION,
                self->temp_cfg_.device_name.c_str());
            response = info;
            break;
        }

        case IDX_DIAG_INFO: {
            if (level < BluetoothService::AccessLevel::LEVEL_2)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;

            const esp_partition_t* running = esp_ota_get_running_partition();
            char diag[384];
            snprintf(diag, sizeof(diag),
                "{\"free_heap\":%lu,\"uptime_s\":%lu,\"fw_partition\":\"%s\"}",
                (unsigned long)esp_get_free_heap_size(),
                (unsigned long)(esp_timer_get_time() / 1000000),
                running ? running->label : "unknown");
            response = diag;
            break;
        }

        case IDX_SSID:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            response = self->temp_cfg_.ssid;
            break;

        case IDX_WS_URL:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            response = self->temp_cfg_.ws_url;
            break;

        case IDX_USER_ID:
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            response = self->temp_cfg_.user_id;
            break;

        case IDX_LOG_LEVEL: {
            if (level < BluetoothService::AccessLevel::LEVEL_2)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
            uint8_t lvl = 20; // default INFO
            nvs_handle_t h;
            if (nvs_open("storage", NVS_READONLY, &h) == ESP_OK) {
                nvs_get_u8(h, "log_level", &lvl);
                nvs_close(h);
            }
            os_mbuf_append(ctxt->om, &lvl, 1);
            return 0;
        }

        case IDX_WIFI_SCAN: {
            // Real WiFi networks scanned at boot, served as a JSON array so the
            // app can replace its placeholder list. Requires Level 1 (post-PIN).
            if (level < BluetoothService::AccessLevel::LEVEL_1)
                return BLE_ATT_ERR_INSUFFICIENT_AUTHEN;

            // Cap entries so the value stays within the ATT max (512 bytes).
            constexpr size_t kMaxNetworks = 12;
            response = "[";
            size_t count = 0;
            for (const auto& net : self->wifi_networks_) {
                if (count >= kMaxNetworks) break;
                if (net.ssid.empty()) continue;
                if (count > 0) response += ',';
                response += "{\"ssid\":\"";
                for (char c : net.ssid) {       // minimal JSON string escaping
                    if (c == '"' || c == '\\') response += '\\';
                    response += c;
                }
                response += "\",\"rssi\":";
                response += std::to_string(net.rssi);
                response += '}';
                ++count;
            }
            response += "]";
            break;
        }

        default:
            return BLE_ATT_ERR_UNLIKELY;
        }

        os_mbuf_append(ctxt->om, response.c_str(), response.length());
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

// === NimBLE host task ===

static void ble_host_task(void* param)
{
    ESP_LOGI(TAG, "NimBLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_on_sync(void)
{
    if (BluetoothService::s_instance) {
        BluetoothService::s_instance->start();
    }
}

static void ble_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE reset: reason=%d", reason);
}

// === BluetoothService ===

BluetoothService::BluetoothService()
{
    s_instance = this;
}

BluetoothService::~BluetoothService()
{
    stop();
}

void BluetoothService::generatePairingPin()
{
    uint32_t r = esp_random();
    snprintf(pairing_pin_, sizeof(pairing_pin_), "%06lu", (unsigned long)(r % 1000000));
    ESP_LOGI(TAG, "Pairing PIN: %s", pairing_pin_);
}

bool BluetoothService::verifyAdminToken(const uint8_t* data, size_t len)
{
    // Expected format: 32-byte HMAC token + 4-byte LE timestamp
    if (len != 36) return false;

    // Load admin_secret from NVS
    nvs_handle_t h;
    char secret[64] = {};
    size_t secret_len = sizeof(secret);
    if (nvs_open("storage", NVS_READONLY, &h) != ESP_OK) return false;
    esp_err_t err = nvs_get_str(h, "admin_secret", secret, &secret_len);
    nvs_close(h);
    if (err != ESP_OK || secret_len == 0) return false;

    uint32_t ts = data[32] | (data[33] << 8) | (data[34] << 16) | (data[35] << 24);

    // Build message: mac || timestamp_str
    const char* mac = getDLuniceEfuseID();
    char ts_str[16];
    snprintf(ts_str, sizeof(ts_str), "%lu", (unsigned long)ts);

    char msg[192];
    size_t msg_len = snprintf(msg, sizeof(msg), "%s%s", mac, ts_str);

    // HMAC-SHA256(msg, admin_secret)
    uint8_t expected[32];
    mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                    (const uint8_t*)secret, strlen(secret),
                    (const uint8_t*)msg, msg_len,
                    expected);

    if (memcmp(data, expected, 32) != 0) return false;

    // Timestamp within ±5 minutes (300s)
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000000);
    int32_t diff = (int32_t)ts - (int32_t)now;
    if (diff < -300 || diff > 300) {
        ESP_LOGW(TAG, "Admin token timestamp out of range: diff=%ld", (long)diff);
        return false;
    }

    return true;
}

void BluetoothService::notifyCommandResult(uint8_t result)
{
    if (!connected_) return;
    struct os_mbuf* om = ble_hs_mbuf_from_flat(&result, 1);
    if (om) {
        ble_gatts_notify_custom(conn_handle_, chr_val_handles[IDX_COMMAND], om);
    }
}

void BluetoothService::notifyCommitResult(uint8_t result)
{
    if (!connected_) return;
    struct os_mbuf* om = ble_hs_mbuf_from_flat(&result, 1);
    if (om) {
        ble_gatts_notify_custom(conn_handle_, chr_val_handles[IDX_COMMIT], om);
    }
}

void BluetoothService::executeCommand(BleCommand cmd)
{
    switch (cmd) {
    case BleCommand::RESTART:
        ESP_LOGW(TAG, "CMD: RESTART");
        esp_restart();
        break;

    case BleCommand::FACTORY_RESET:
        ESP_LOGW(TAG, "CMD: FACTORY_RESET");
        factoryReset();
        esp_restart();
        break;

    case BleCommand::FULL_WIPE:
        ESP_LOGW(TAG, "CMD: FULL_WIPE");
        fullWipe();
        esp_restart();
        break;

    case BleCommand::ROLLBACK_FW: {
        ESP_LOGW(TAG, "CMD: ROLLBACK_FW");
        const esp_partition_t* prev = esp_ota_get_last_invalid_partition();
        if (prev) {
            esp_ota_set_boot_partition(prev);
            esp_restart();
        }
        break;
    }

    case BleCommand::ENABLE_DEBUG: {
        ESP_LOGI(TAG, "CMD: ENABLE_DEBUG");
        esp_log_level_set("*", ESP_LOG_DEBUG);
        break;
    }

    case BleCommand::DISABLE_DEBUG:
        ESP_LOGI(TAG, "CMD: DISABLE_DEBUG");
        esp_log_level_set("*", ESP_LOG_INFO);
        break;

    case BleCommand::CLEAR_USERS: {
        ESP_LOGW(TAG, "CMD: CLEAR_USERS");
        nvs_handle_t h;
        if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_key(h, "user_id");
            nvs_commit(h);
            nvs_close(h);
        }
        break;
    }

    case BleCommand::ENTER_DFU:
        ESP_LOGW(TAG, "CMD: ENTER_DFU (not implemented)");
        break;
    }
}

void BluetoothService::factoryReset()
{
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_key(h, "ssid");
        nvs_erase_key(h, "pass");
        nvs_erase_key(h, "ws_url");
        nvs_erase_key(h, "user_id");
        nvs_erase_key(h, "device_token");
        nvs_erase_key(h, "admin_secret");
        nvs_commit(h);
        nvs_close(h);
    }
}

void BluetoothService::fullWipe()
{
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool BluetoothService::init(const std::string& adv_name,
                             const std::vector<WifiInfo>& cached_networks,
                             const ConfigData* current_config)
{
    if (initialized_) return true;

    s_instance = this;
    adv_name_ = adv_name;
    if (current_config) temp_cfg_ = *current_config;

    device_id_str_ = getDLuniceEfuseID();
    generatePairingPin();

    // Prepare WiFi list (sorted by RSSI, deduplicated)
    wifi_networks_ = cached_networks;
    std::sort(wifi_networks_.begin(), wifi_networks_.end(),
              [](const WifiInfo& a, const WifiInfo& b) { return a.rssi > b.rssi; });
    std::vector<WifiInfo> cleaned;
    std::set<std::string> seen;
    for (auto& net : wifi_networks_) {
        if (!net.ssid.empty() && seen.find(net.ssid) == seen.end()) {
            cleaned.push_back(net);
            seen.insert(net.ssid);
        }
    }
    wifi_networks_ = cleaned;

    // Initialize NimBLE
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return false;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return false;
    }

    ble_svc_gap_device_name_set(adv_name_.c_str());
    nimble_port_freertos_init(ble_host_task);

    initialized_ = true;
    ESP_LOGI(TAG, "BLE initialized: %s (PIN: %s)", adv_name_.c_str(), pairing_pin_);
    return true;
}

bool BluetoothService::start()
{
    if (started_) return true;

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    ble_uuid16_t adv_svc_uuid = BLE_UUID16_INIT(0xFF01);
    fields.uuids16 = &adv_svc_uuid;
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return false;
    }

    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.name = (uint8_t*)adv_name_.c_str();
    rsp_fields.name_len = adv_name_.length();
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return false;
    }

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, ble_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return false;
    }

    started_ = true;
    ESP_LOGI(TAG, "BLE Advertising: %s", adv_name_.c_str());
    return true;
}

void BluetoothService::stop()
{
    if (!started_) return;
    ble_gap_adv_stop();
    started_ = false;
    ESP_LOGI(TAG, "BLE stopped");
}

void BluetoothService::deinit()
{
    stop();

    int rc = nimble_port_stop();
    if (rc == 0) {
        nimble_port_deinit();
    }

    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    ESP_LOGI(TAG, "BLE controller memory released");

    initialized_ = false;
    s_instance = nullptr;
}
