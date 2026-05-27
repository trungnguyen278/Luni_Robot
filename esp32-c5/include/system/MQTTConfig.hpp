#pragma once

#include <cstdint>
#include <string>

/**
 * MQTTConfig.hpp
 * ---------------------------------------------------------
 * MEO SDK Compatible MQTT Configuration
 * 
 * This file defines the MQTT message formats and topic patterns
 * compatible with the MEO SDK protocol.
 * 
 * Topic Patterns:
 * - Cloud invoke: meo/{userId}/{dLuniceId}/feature
 * - Legacy invoke: meo/{dLuniceId}/feature/{featureName}/invoke
 * - Event publish: meo/{userId}/{dLuniceId}/event/{eventName}
 * - Feature response: meo/{userId}/{dLuniceId}/event/feature_response
 * 
 * Legacy Luni topics (backward compatibility):
 * - dLunices/{MAC}/cmd - config commands
 * - dLunices/{MAC}/status - dLunice status
 * - dLunices/{MAC}/ota_data - OTA firmware chunks
 * - dLunices/{MAC}/ota_ack - OTA acknowledgments
 */

namespace mqtt_config
{
    // =========================================================================
    // MESSAGE TYPES (Legacy Luni commands, mapped to MEO features)
    // =========================================================================
    
    /**
     * Config command types sent from server to dLunice
     * These are legacy commands that map to MEO feature invokes
     */
    enum class ConfigCommand : uint8_t
    {
        INVALID = 0,
        // DLunice info handshake
        DLuniCE_HANDSHAKE = 1,      // DLunice initiates handshake with server (sends dLunice_id)
        SET_WIFI = 2,              // NOT allowed over MQTT (respond not_supported); use BLE
        SET_AUDIO_VOLUME = 3,      // Server → DLunice: Set speaker volume (0-100)
        SET_BRIGHTNESS = 4,        // Server → DLunice: Set display brightness (0-100)
        SET_DLuniCE_NAME = 5,       // Server → DLunice: Set dLunice name
        SET_WS_URL = 6,            // Server → DLunice: Update WebSocket server URL
        REBOOT = 7,                // Server → DLunice: Request reboot
        REQUEST_STATUS = 8,        // Server → DLunice: Request dLunice status
        REQUEST_OTA = 9,           // Server → DLunice: Trigger OTA update (optional version)
        REQUEST_BLE_CONFIG = 10,   // Server → DLunice: Open BLE config mode with WiFi scan
        
        // Luni-specific commands
        SET_EMOTION = 11,          // Server → DLunice: Set display emotion
        PLAY_TTS = 12,             // Server → DLunice: Play TTS audio
        STOP_AUDIO = 13,           // Server → DLunice: Stop audio playback
    };

    /**
     * Response status codes
     */
    enum class ResponseStatus : uint8_t
    {
        OK = 0,
        ERROR = 1,
        INVALID_COMMAND = 2,
        INVALID_PARAM = 3,
        NOT_SUPPORTED = 4,
        DLuniCE_BUSY = 5
    };

    // =========================================================================
    // JSON MESSAGE FORMATS
    // =========================================================================

    /**
     * DLunice Handshake (DLunice → Server on WS connect)
     * The dLunice sends this once after the WebSocket connects.
     * Response does not include a status field.
     * {
     *   "cmd": "dLunice_handshake",
     *   "dLunice_id": "A1B2C3D4E5F6",      // MAC address
     *   "firmware_version": "1.0.0",
     *   "dLunice_name": "Luni-DLunice",
     *   "battery_percent": 85,
     *   "connectivity_state": "ONLINE"
     * }
     */
    
    /**
     * Set WiFi Config (Server → DLunice)
     * NOT supported via WebSocket for stability/security. Use BLE portal.
     * Request:
     * {
     *   "cmd": "set_wifi",
     *   "ssid": "MyNetwork",
     *   "password": "MyPassword"
     * }
     * Response:
     * {
     *   "status": "not_supported",
     *   "message": "WiFi config not supported over WebSocket. Use BLE.",
     *   "dLunice_id": "A1B2C3D4E5F6"
     * }
     */

    /**
     * Set Volume (Server → DLunice)
     * Persists to NVS and applies immediately.
     * {
     *   "cmd": "set_volume",
     *   "volume": 75        // 0-100
     * }
     * Response:
     * {
     *   "status": "ok" | "error",
     *   "volume": 75,
     *   "dLunice_id": "A1B2C3D4E5F6"
     * }
     */

    /**
     * Set Brightness (Server → DLunice)
     * Persists to NVS and applies immediately.
     * {
     *   "cmd": "set_brightness",
     *   "brightness": 80    // 0-100
     * }
     * Response:
     * {
     *   "status": "ok" | "error",
     *   "brightness": 80,
     *   "dLunice_id": "A1B2C3D4E5F6"
     * }
     */

    /**
     * Set DLunice Name (Server → DLunice)
     * Persists to NVS and applies immediately (affects BLE name on next init if wired).
     * {
     *   "cmd": "set_dLunice_name",
     *   "dLunice_name": "Living Room Speaker"
     * }
     * Response:
     * {
     *   "status": "ok" | "error",
     *   "dLunice_name": "Living Room Speaker",
     *   "dLunice_id": "A1B2C3D4E5F6"
     * }
     */

    /**
     * Reboot (Server → DLunice)
     * DLunice acknowledges and then restarts.
     * Request:
     * {
     *   "cmd": "reboot"
     * }
     * Response:
     * {
     *   "status": "ok",
     *   "message": "Rebooting..."
     * }
     */

    /**
     * Set WebSocket URL (Server → DLunice)
     * NOT implemented currently. Reserved for future use.
     * If implemented later, dLunice should persist the URL and reconnect.
     * Request:
     * {
     *   "cmd": "set_ws_url",
     *   "url": "ws://host:port/ws"
     * }
     */

    /**
     * Request Status (Server → DLunice)
     * {
     *   "cmd": "request_status"
     * }
     * Response:
     * {
     *   "status": "ok",
     *   "dLunice_id": "A1B2C3D4E5F6",
     *   "dLunice_name": "Luni-DLunice",
     *   "battery_percent": 85,
     *   "connectivity_state": "ONLINE",
     *   "volume": 75,
     *   "brightness": 80,
     *   "firmware_version": "1.0.0",
     *   "uptime_sec": 3600
     * }
     */

    // =========================================================================
    // Helper Functions
    // =========================================================================

    /**
     * Parse command string to ConfigCommand enum
     */
    inline ConfigCommand parseCommandString(const std::string &cmd_str)
    {
        if (cmd_str == "dLunice_handshake")
            return ConfigCommand::DLuniCE_HANDSHAKE;
        if (cmd_str == "set_wifi")
            return ConfigCommand::SET_WIFI;
        if (cmd_str == "set_volume")
            return ConfigCommand::SET_AUDIO_VOLUME;
        if (cmd_str == "set_brightness")
            return ConfigCommand::SET_BRIGHTNESS;
        if (cmd_str == "set_dLunice_name")
            return ConfigCommand::SET_DLuniCE_NAME;
        if (cmd_str == "set_ws_url")
            return ConfigCommand::SET_WS_URL;
        if (cmd_str == "reboot")
            return ConfigCommand::REBOOT;
        if (cmd_str == "request_status")
            return ConfigCommand::REQUEST_STATUS;
        if (cmd_str == "request_ota")
            return ConfigCommand::REQUEST_OTA;
        if (cmd_str == "request_ble_config")
            return ConfigCommand::REQUEST_BLE_CONFIG;
        if (cmd_str == "set_emotion")
            return ConfigCommand::SET_EMOTION;
        if (cmd_str == "play_tts")
            return ConfigCommand::PLAY_TTS;
        if (cmd_str == "stop_audio")
            return ConfigCommand::STOP_AUDIO;
        
        return ConfigCommand::INVALID;
    }

    /**
     * Get string representation of ConfigCommand
     */
    inline const char *commandToString(ConfigCommand cmd)
    {
        switch (cmd)
        {
        case ConfigCommand::DLuniCE_HANDSHAKE:
            return "dLunice_handshake";
        case ConfigCommand::SET_WIFI:
            return "set_wifi";
        case ConfigCommand::SET_AUDIO_VOLUME:
            return "set_volume";
        case ConfigCommand::SET_BRIGHTNESS:
            return "set_brightness";
        case ConfigCommand::SET_DLuniCE_NAME:
            return "set_dLunice_name";
        case ConfigCommand::SET_WS_URL:
            return "set_ws_url";
        case ConfigCommand::REBOOT:
            return "reboot";
        case ConfigCommand::REQUEST_STATUS:
            return "request_status";
        case ConfigCommand::REQUEST_OTA:
            return "request_ota";
        case ConfigCommand::REQUEST_BLE_CONFIG:
            return "request_ble_config";
        case ConfigCommand::SET_EMOTION:
            return "set_emotion";
        case ConfigCommand::PLAY_TTS:
            return "play_tts";
        case ConfigCommand::STOP_AUDIO:
            return "stop_audio";
        default:
            return "invalid";
        }
    }

    /**
     * Get string representation of ResponseStatus
     */
    inline const char *statusToString(ResponseStatus status)
    {
        switch (status)
        {
        case ResponseStatus::OK:
            return "ok";
        case ResponseStatus::ERROR:
            return "error";
        case ResponseStatus::INVALID_COMMAND:
            return "invalid_command";
        case ResponseStatus::INVALID_PARAM:
            return "invalid_param";
        case ResponseStatus::NOT_SUPPORTED:
            return "not_supported";
        case ResponseStatus::DLuniCE_BUSY:
            return "dLunice_busy";
        default:
            return "unknown";
        }
    }

} // namespace mqtt_config
