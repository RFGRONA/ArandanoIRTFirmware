// src/ConfigManager.cpp
#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "LEDStatus.h" // Included if initFilesystem uses it

// Define the global 'config' instance
Config config;

// Define CONFIG_FILENAME, which was previously in main.cpp
#define CONFIG_FILENAME "/config.json"

/**
 * @brief Initializes and mounts the LittleFS filesystem.
 * Attempts to mount the filesystem. If mounting fails, it attempts to
 * format the filesystem and then mount it again.
 * @param statusLed Optional pointer to an LEDStatus object for visual error indication.
 * @return `true` if the filesystem is successfully mounted.
 * `false` if mounting and formatting both fail.
 */
bool initFilesystem() { // Matched prototype
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Initializing LittleFS filesystem...");
    #endif
    if (!LittleFS.begin(false)) { // false = do not format if mount fails on the first try
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] Warning: Initial LittleFS mount failed! Attempting to format...");
        #endif
        delay(1000); // Give it a moment
        if (!LittleFS.begin(true)) { // true = format if mount still fails
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[ConfigMgr] CRITICAL ERROR: Formatting LittleFS failed! Check hardware/partition scheme.");
            #endif
            return false;
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[ConfigMgr] LittleFS formatted successfully. Filesystem is now empty.");
            #endif
        }
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] LittleFS mounted successfully.");
        // Optional: List directory contents or show FS info
        // Serial.printf(" FS Info: Total bytes: %lu, Used bytes: %lu\n", LittleFS.totalBytes(), LittleFS.usedBytes());
    #endif
    return true;
}

/**
 * @brief Loads configuration from a JSON file on LittleFS into the global 'config' struct.
 * Reads the specified file, parses its JSON content, and updates the members
 * of the global `config` object. If the file doesn't exist, cannot be opened,
 * or contains invalid JSON, default values already present in the `config` struct
 * will be used.
 * @param filename The full path to the configuration file on the LittleFS filesystem.
 * @return `true` if the configuration file was successfully opened, parsed, and values were extracted.
 * `false` otherwise.
 */
bool loadConfiguration(const char *filename) {
    File configFile = LittleFS.open(filename, "r");
    if (!configFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ConfigMgr] Warning: Failed to open config file: %s. Using defaults.\n", filename);
        #endif
        return false;
    }

    JsonDocument doc; // Using dynamic allocation by default for ArduinoJson v6+

    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("[ConfigMgr] Warning: Failed to parse config file JSON: ");
            Serial.println(error.c_str());
            Serial.println("[ConfigMgr] Using default configuration values.");
        #endif
        return false;
    }

    // Extract configuration values. Use default from 'config' object if key is missing.
    config.wifi_ssid = doc["wifi_ssid"] | config.wifi_ssid;
    config.wifi_pass = doc["wifi_pass"] | config.wifi_pass;

    config.deviceId = doc["FIRMWARE_DEVICE_ID"] | config.deviceId; // Matched your original key
    config.activationCode = doc["FIRMWARE_ACTIVATION_CODE"] | config.activationCode; // Matched your original key

    config.apiBaseUrl = doc["api_base_url"] | config.apiBaseUrl;
    config.apiActivatePath = doc["api_activate_path"] | config.apiActivatePath;
    config.apiAuthPath = doc["api_auth_path"] | config.apiAuthPath;
    config.apiRefreshTokenPath = doc["api_refresh_token_path"] | config.apiRefreshTokenPath;
    config.apiLogPath = doc["api_log_path"] | config.apiLogPath;
    config.apiAmbientDataPath = doc["api_ambient_data_path"] | config.apiAmbientDataPath;
    config.apiCaptureDataPath = doc["api_capture_data_path"] | config.apiCaptureDataPath;

    config.sleep_sec = doc["sleep_sec"] | config.sleep_sec;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Configuration loaded successfully from file:");
        Serial.println("  WiFi SSID: " + config.wifi_ssid);
        // Serial.println("  WiFi Pass: [REDACTED]"); // Keep password redacted
        Serial.println("  Device ID: " + String(config.deviceId));
        // Serial.println("  Activation Code: [REDACTED]");
        Serial.println("  API Base URL: " + config.apiBaseUrl);
        Serial.println("  API Activate Path: " + config.apiActivatePath);
        Serial.println("  API Auth Path: " + config.apiAuthPath);
        Serial.println("  API Refresh Token Path: " + config.apiRefreshTokenPath);
        Serial.println("  API Log Path: " + config.apiLogPath);
        Serial.println("  API Ambient Data Path: " + config.apiAmbientDataPath);
        Serial.println("  API Capture Data Path: " + config.apiCaptureDataPath);
        Serial.println("  Sleep Seconds: " + String(config.sleep_sec));
    #endif
    return true;
}

/**
 * @brief Loads configuration from the default config file.
 * This is a convenience wrapper around loadConfiguration().
 * It does NOT set WiFi credentials; that should be done in main after calling this.
 */
void loadConfigurationFromFile() { // Renamed from loadConfigAndSetCredentials
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Loading configuration from default file: " + String(CONFIG_FILENAME));
    #endif
    if (!loadConfiguration(CONFIG_FILENAME)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] Warning: loadConfiguration failed or file not found. Using default config values.");
        #endif
        // 'config' struct will retain its default values if loading fails.
    }
}