/**
 * @file API.h
 * @brief Defines the API class for handling device communication with the AIRT backend.
 *
 * This class manages device activation, authentication token retrieval and refresh,
 * and stores essential operational data (tokens, activation state, data collection interval)
 * in Non-Volatile Storage (NVS). It provides methods to interact with specific API endpoints.
 * All HTTP communication and NVS operations are encapsulated within this class.
 * It does NOT directly control LEDs; status should be inferred from method return values.
 */
#ifndef API_H
#define API_H

#include <Arduino.h>
#include <Preferences.h> // For NVS access
#include <HTTPClient.h>  // For making HTTP requests
#include <ArduinoJson.h> // For JSON parsing and serialization

// NVS Namespace and Keys
#define NVS_NAMESPACE "airt_config"
#define KEY_ACCESS_TOKEN "acc_token"
#define KEY_REFRESH_TOKEN "ref_token"
#define KEY_DATA_COLLECTION_TIME "coll_time"
#define KEY_IS_ACTIVATED "is_active"
#define KEY_NVS_VALID_FLAG "nvs_ok" 

// Default value for data collection time if not set by backend
#define DEFAULT_DATA_COLLECTION_MINUTES 60

class API {
public:
    /**
     * @brief Constructor for the API class.
     * Initializes API endpoint paths and loads persistent data from NVS.
     * @param base_url The base URL of the AIRT API.
     * @param activate_path The relative path for the device activation endpoint.
     * @param auth_path The relative path for the device authentication/status check endpoint.
     * @param refresh_path The relative path for the token refresh endpoint.
     */
    API(const String& base_url, const String& activate_path, const String& auth_path, const String& refresh_path);

    /**
     * @brief Destructor. Closes Preferences.
     */
    ~API();

    /**
     * @brief Checks if the device is currently marked as activated.
     * @return True if the device has successfully activated and stored its state, false otherwise.
     */
    bool isActivated() const;

    /**
     * @brief Retrieves the current stored access token.
     * @return The access token as a String. May be empty if no token is stored or valid.
     */
    String getAccessToken() const;

    /**
     * @brief Retrieves the configured base URL for the API.
     * @return The base API URL as a String.
     */
    String getBaseApiUrl() const;

    /**
     * @brief Retrieves the current data collection interval in minutes.
     * This value is typically obtained from the backend.
     * @return The data collection interval in minutes. Returns a default if not set.
     */
    int getDataCollectionTimeMinutes() const;

    /**
     * @brief Attempts to activate the device with the backend.
     * Sends the device ID and activation code to the activation endpoint.
     * On success, stores the received access token, refresh token, and data collection time.
     * Sets the device activation status in NVS.
     * @param deviceId The unique ID of the device.
     * @param activationCode The activation code for the device.
     * @return An integer HTTP status code from the activation attempt. 200 for success.
     * Returns negative values for client-side errors (e.g., HTTPClient errors).
     */
    int performActivation(const String& deviceId, const String& activationCode);

    /**
     * @brief Checks the backend status and device authentication by calling the '/auth' endpoint.
     * Uses the current access token for authorization. If the token is invalid (HTTP 401),
     * it attempts to refresh the token using performTokenRefresh().
     * Updates tokens and data collection time if they differ from the backend's response.
     * @return An integer HTTP status code from the auth attempt. 200 for success.
     * If a 401 occurs and token refresh is attempted, the status of the refresh attempt is returned.
     * Returns negative values for client-side errors.
     */
    int checkBackendAndAuth();

    /**
     * @brief Attempts to refresh the access token using the stored refresh token.
     * Calls the '/refresh-token' endpoint.
     * On success, updates the stored access token, refresh token, and data collection time.
     * If the refresh token is invalid (HTTP 401), it deactivates the device.
     * @return An integer HTTP status code from the refresh attempt. 200 for success.
     * Returns negative values for client-side errors.
     */
    int performTokenRefresh();

    /**
     * @brief Sets the device's MAC address.
     * This MAC address will be used during the activation process.
     * @param mac The MAC address of the device as a String.
     */
    void setDeviceMAC(const String& mac);

    /**
     * @brief Closes the Preferences (NVS) namespace.
     * Should be called before entering deep sleep to ensure data is committed.
     */
    void closePreferences();

private:
    Preferences _preferences; ///< Preferences instance for NVS operations.

    // Configuration obtained from constructor (from main.cpp's Config struct)
    String _apiBaseUrl;
    String _apiActivatePath;
    String _apiAuthPath;
    String _apiRefreshTokenPath;

    // In-memory state variables, loaded from NVS at startup
    String _accessToken;
    String _refreshToken;
    int _dataCollectionTimeMinutes;
    bool _activatedFlag;
    String _deviceMACAddress;

    /**
     * @brief Loads all persistent data items from NVS into member variables.
     * Called by the constructor.
     */
    void _loadPersistentData();

    /**
     * @brief Saves the current access token to NVS and updates the member variable.
     * @param token The access token to save.
     */
    void _saveAccessToken(const String& token);

    /**
     * @brief Saves the current refresh token to NVS and updates the member variable.
     * @param token The refresh token to save.
     */
    void _saveRefreshToken(const String& token);

    /**
     * @brief Saves the data collection time (in minutes) to NVS and updates the member variable.
     * @param minutes The data collection interval in minutes.
     */
    void _saveDataCollectionTime(int minutes);

    /**
     * @brief Saves the activation status to NVS and updates the member variable.
     * @param activated The activation status (true or false).
     */
    void _saveActivationStatus(bool activated);

    /**
     * @brief Performs an HTTP POST request with a JSON payload.
     * @param fullUrl The complete URL for the POST request.
     * @param authorizationToken The token to use for the 'Authorization: Device <token>' header. Can be empty if no auth needed.
     * @param jsonPayload The JSON string to send as the request body.
     * @param responsePayload Buffer to store the HTTP response payload.
     * @return The HTTP status code, or a negative value for client errors.
     */
    int _httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload);

    /**
     * @brief Parses the common DeviceAuthResponseDto or DeviceActivationResponseDto.
     * Extracts accessToken, refreshToken, and dataCollectionTimeMinutes.
     * Updates stored values if new ones are different.
     * @param jsonResponse The JSON string received from the API.
     * @return True if parsing was successful and required fields were found, false otherwise.
     */
    bool _parseAndStoreAuthResponse(const String& jsonResponse);
};

#endif // API_H