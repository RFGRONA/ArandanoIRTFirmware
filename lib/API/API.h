// lib/API/API.h
#ifndef API_H
#define API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SDManager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_random.h" 
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "ErrorLogger.h"
#include "TimeManager.h"
#include "ConfigManager.h"

// Define the AES key size in bytes (e.g., 32 for AES-256, 16 for AES-128)
#define API_AES_KEY_SIZE 32

// Define the filename for storing API state on SD card
#define API_STATE_FILE_PATH SDManager::API_STATE_FILENAME

class API {
public:
    /**
     * @brief Constructor for the API class.
     * Initializes API endpoint paths and loads persistent data from SD card via SDManager.
     * @param sdManager Reference to an initialized SDManager instance.
     * @param base_url The base URL of the AIRT API.
     * @param activate_path The relative path for the device activation endpoint.
     * @param auth_path The relative path for the device authentication/status check endpoint.
     * @param refresh_path The relative path for the token refresh endpoint.
     */
    API(SDManager& sdManager, 
        const String& base_url, 
        const String& activate_path, 
        const String& auth_path, 
        const String& refresh_path);

    /**
     * @brief Destructor.
     */
    ~API();

    bool isActivated() const;
    String getAccessToken() const;
    String getBaseApiUrl() const;
    int getDataCollectionTimeMinutes() const;

    int performActivation(const String& deviceId, const String& activationCode);
    int checkBackendAndAuth();
    int performTokenRefresh();
    void setDeviceMAC(const String& mac);
    

private:
    SDManager& _sdManagerRef; 

    String _apiBaseUrl;
    String _apiActivatePath;
    String _apiAuthPath;
    String _apiRefreshTokenPath;

    String _accessToken;
    String _refreshToken;
    int _dataCollectionTimeMinutes;
    bool _activatedFlag;
    String _deviceMACAddress;

    uint8_t _aesKey[API_AES_KEY_SIZE]; // To store the AES key in memory
    bool _aesKeyInitialized;

    /**
     * @brief Loads API state (tokens, activation status, etc.) from SD card.
     * Called by the constructor. If loading fails or file doesn't exist,
     * initializes with default values and attempts to save a new state file.
     */
    void _loadPersistentData();

    /**
     * @brief Saves the current API state (all relevant member variables) to SD card.
     * This will be a new private helper method.
     * @return True if saving was successful, false otherwise.
     */
    bool _saveCurrentApiStateToSd();

    /**
     * @brief Initializes the AES key.
     * Tries to load it from NVS. If not found, generates a new one and saves it to NVS.
     * @return True if key is successfully initialized/loaded, false otherwise.
     */
    bool _initAesKey();

    // Individual save methods will now update member variables and then call _saveCurrentApiStateToSd()
    void _updateAccessToken(const String& token);
    void _updateRefreshToken(const String& token);
    void _updateDataCollectionTime(int minutes);
    void _updateActivationStatus(bool activated);

    int _httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload);
    bool _parseAndStoreAuthResponse(const String& jsonResponse); 
};

#endif // API_H