// src/ConfigManager.h
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h> 
// --- Configuration Structure ---
/**
 * @brief Holds application configuration parameters loaded from LittleFS or default values.
 */
struct Config {
    String wifi_ssid = "DEFAULT_SSID";
    String wifi_pass = "";
    int deviceId = 0;
    String activationCode = "";
    String apiBaseUrl = "";
    String apiActivatePath = "/api/device-api/activate";
    String apiAuthPath = "/api/device-api/auth";
    String apiRefreshTokenPath = "/api/device-api/refresh-token";
    String apiLogPath = "/api/device-api/log";
    String apiAmbientDataPath = "/api/device-api/ambient-data";
    String apiCaptureDataPath = "/api/device-api/capture-data";
    int data_interval_minutes = 30;
};

extern Config config; // Declare 'config' as an external global variable

// --- Function Prototypes for ConfigManager ---
/** @brief Loads configuration from LittleFS into the global 'config' struct.
 * @param filename The name of the configuration file.
 * @return True if configuration loaded successfully, false otherwise.
 */
bool loadConfiguration(const char *filename);

/** @brief Initializes and mounts the LittleFS filesystem, formatting if necessary.
 * @param statusLed Pointer to an LEDStatus object to indicate errors. Can be nullptr if no LED feedback is needed here.
 * @return True if filesystem is successfully mounted.
 */
bool initFilesystem(); 

/** @brief Loads configuration from the default config file.
 * This is a convenience wrapper around loadConfiguration().
 * It does NOT set WiFi credentials directly in WiFiManager.
 */
void loadConfigurationFromFile();

#endif // CONFIG_MANAGER_H