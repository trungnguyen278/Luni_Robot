#pragma once

#include <string>
#include <vector>
#include <functional>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"

/**
 * WifiInfo - Kết quả scan wifi
 */
struct WifiInfo {
    std::string ssid;
    int rssi = -99;
};

/**
 * WifiService (STA-only for ESP32-C5)
 * ---------------------------------------------------------
 * - Init WiFi stack (NVS, netif, wifi driver)
 * - STA connect with credentials
 * - Scan WiFi networks (for BLE provisioning listing)
 * - Callback: 0=DISCONNECTED, 1=CONNECTING, 2=GOT_IP
 */
class WifiService {
public:
    WifiService() = default;
    ~WifiService() = default;

    void init();

    // Connect with saved credentials from NVS
    bool autoConnect();

    // Connect with explicit credentials
    void connectWithCredentials(const char* ssid, const char* pass);

    // Disconnect
    void disconnect();

    // Status
    bool isConnected() const { return connected; }
    std::string getIp() const;
    std::string getSsid() const { return sta_ssid; }

    // Raw wifi_err_reason_t from the last STA_DISCONNECTED event (0 = none yet).
    // Lets NetworkManager map a connect failure to the right ConnectFailReason
    // (wrong password vs AP not found vs DHCP) instead of always WIFI_TIMEOUT.
    int lastDisconnectReason() const { return last_disconnect_reason_; }
    // True once the link layer associated (STA_CONNECTED) for the current
    // attempt — if we then time out without an IP, the failure is DHCP, not auth.
    bool wasAssociated() const { return associated_; }

    // Scan
    std::vector<WifiInfo> scanNetworks();
    void ensureStaStarted();

    // Callback status: 0=DISCONNECTED, 1=CONNECTING, 2=GOT_IP
    void onStatus(std::function<void(int)> cb) { status_cb = cb; }

private:
    void loadCredentials();
    void saveCredentials(const char* ssid, const char* pass);
    void startSTA();
    void registerEvents();

    static void wifiEventHandlerStatic(void* arg, esp_event_base_t base,
                                       int32_t id, void* data);
    static void ipEventHandlerStatic(void* arg, esp_event_base_t base,
                                     int32_t id, void* data);
    void wifiEventHandler(esp_event_base_t base, int32_t id, void* data);
    void ipEventHandler(esp_event_base_t base, int32_t id, void* data);

private:
    std::string sta_ssid;
    std::string sta_pass;

    bool connected = false;
    bool wifi_started = false;

    // Updated from the WiFi event handler (other task context); plain ints are
    // fine here — single writer, read after the connect window closes.
    volatile int  last_disconnect_reason_ = 0;
    volatile bool associated_ = false;

    esp_netif_t* sta_netif = nullptr;

    std::function<void(int)> status_cb = nullptr;
};
