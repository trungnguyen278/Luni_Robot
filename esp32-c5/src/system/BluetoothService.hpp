#pragma once

#include <string>
#include <functional>
#include <vector>
#include <cstdint>
#include <atomic>

#include "Version.hpp"
#include "WifiService.hpp"

class BluetoothService
{
public:
    struct ConfigData
    {
        std::string device_name = "Luni";
        uint8_t volume = 60;
        uint8_t brightness = 100;
        std::string ssid;
        std::string pass;
        std::string ws_url;
        std::string user_id;
        std::string device_token;
        std::string admin_secret;
        ConfigData() = default;
    };

    using OnConfigComplete = std::function<void(const ConfigData&)>;

    BluetoothService();
    ~BluetoothService();

    bool init(const std::string& adv_name,
              const std::vector<WifiInfo>& cached_networks = {},
              const ConfigData* current_config = nullptr);
    bool start();
    void stop();
    void deinit();

    using OnConnectionChange = std::function<void(bool connected)>;

    void onConfigComplete(OnConfigComplete cb) { config_cb_ = cb; }
    void onConnectionChange(OnConnectionChange cb) { conn_cb_ = cb; }
    const char* getPairingPin() const { return pairing_pin_; }

    // Access levels
    enum class AccessLevel : uint8_t { LEVEL_0 = 0, LEVEL_1 = 1, LEVEL_2 = 2 };

    // Service UUID
    static constexpr uint16_t SVC_UUID = 0xFF01;

    // Characteristic UUIDs (plan §11.2)
    static constexpr uint16_t CHR_SSID         = 0x0001;
    static constexpr uint16_t CHR_PASSWORD      = 0x0002;
    static constexpr uint16_t CHR_WS_URL        = 0x0003;
    static constexpr uint16_t CHR_COMMIT         = 0x0004;
    static constexpr uint16_t CHR_USER_ID       = 0x0005;
    static constexpr uint16_t CHR_DEV_TOKEN     = 0x0008;
    static constexpr uint16_t CHR_WIFI_SCAN     = 0x0009;
    static constexpr uint16_t CHR_DEVICE_INFO   = 0x0006;
    static constexpr uint16_t CHR_DIAG_INFO     = 0x0007;
    static constexpr uint16_t CHR_AUTH_UNLOCK   = 0x0010;
    static constexpr uint16_t CHR_COMMAND       = 0x0011;
    static constexpr uint16_t CHR_ADMIN_AUTH    = 0x0012;
    static constexpr uint16_t CHR_LOG_LEVEL     = 0x0013;
    static constexpr uint16_t CHR_ADMIN_SECRET  = 0x0014;

    // BLE commands (plan §11.3)
    enum class BleCommand : uint8_t {
        RESTART         = 0x01,
        FACTORY_RESET   = 0x10,
        FULL_WIPE       = 0x11,
        ROLLBACK_FW     = 0x12,
        ENABLE_DEBUG    = 0x13,
        DISABLE_DEBUG   = 0x14,
        CLEAR_USERS     = 0x15,
        ENTER_DFU       = 0x16,
    };

    // Command response codes
    static constexpr uint8_t CMD_OK           = 0x00;
    static constexpr uint8_t CMD_FAIL         = 0x01;
    static constexpr uint8_t CMD_UNAUTHORIZED = 0x02;
    static constexpr uint8_t CMD_LOCKED       = 0x03;  // PIN locked out (too many tries)

    // NimBLE callbacks need access to instance
    static BluetoothService* s_instance;

    ConfigData temp_cfg_;
    OnConfigComplete config_cb_ = nullptr;
    OnConnectionChange conn_cb_ = nullptr;
    std::atomic<AccessLevel> access_level_{AccessLevel::LEVEL_0};

    std::string device_id_str_;
    std::vector<WifiInfo> wifi_networks_;

    // PIN for Level 1 auth (displayed on robot screen)
    char pairing_pin_[7] = "000000";
    void generatePairingPin();

    // PIN brute-force lockout. The 6-digit space (10^6) is small enough to walk
    // through in a provisioning window, so after MAX_PIN_ATTEMPTS wrong tries we
    // lock all PIN checks for PIN_LOCKOUT_MS. Counter + deadline persist across
    // BLE reconnects (an attacker must not reset the counter by reconnecting);
    // both clear only on a correct PIN. The compare itself is constant-time.
    static constexpr uint8_t  MAX_PIN_ATTEMPTS = 5;
    static constexpr uint32_t PIN_LOCKOUT_MS   = 30000;
    uint8_t pin_attempts_     = 0;
    int64_t pin_lock_until_us_ = 0;
    // Returns true and serves the result if the PIN gate is locked / now locked.
    bool checkPinGate(const uint8_t* buf, uint16_t len);

    // Admin auth (Level 2). Nonce-challenge: the device issues a fresh random
    // nonce (readable on ADMIN_AUTH after Level 1); the app signs mac‖nonce with
    // admin_secret and writes [hmac:32][nonce:4 LE]. Replaces the old
    // timestamp-vs-uptime freshness check, which could never pass (the C5 has no
    // wall clock). The HMAC layout/message/key are unchanged (server + app
    // already match) — only the freshness factor moved from a timestamp to a
    // device-issued nonce, killing replay.
    std::atomic<uint32_t> admin_nonce_{0};
    void regenerateAdminNonce();
    bool verifyAdminToken(const uint8_t* data, size_t len);

    // Current BLE connection handle for notifications
    uint16_t conn_handle_ = 0;
    bool connected_ = false;

    AccessLevel getAccessLevel() const { return access_level_.load(); }

    void executeCommand(BleCommand cmd);
    void notifyCommandResult(uint8_t result);
    void notifyCommitResult(uint8_t result);

    bool started_ = false;
    bool initialized_ = false;

private:
    void factoryReset();
    void fullWipe();

    std::string adv_name_;
};
