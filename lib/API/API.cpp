/**
 * @file API.cpp
 * @brief Implements the API class methods for backend communication and NVS management.
 */
#include "API.h"

// HTTP Timeout (milliseconds)
#define HTTP_REQUEST_TIMEOUT 10000

/**
 * @brief Constructor implementation.
 */
API::API(const String& base_url, const String& activate_path, const String& auth_path, const String& refresh_path) :
    _apiBaseUrl(base_url),
    _apiActivatePath(activate_path),
    _apiAuthPath(auth_path),
    _apiRefreshTokenPath(refresh_path),
    _dataCollectionTimeMinutes(DEFAULT_DATA_COLLECTION_MINUTES), // Initialize with default
    _activatedFlag(false) { // Default to not activated
    _preferences.begin(NVS_NAMESPACE, false); // false for read/write mode
    _loadPersistentData();
}

/**
 * @brief Destructor implementation.
 */
API::~API() {
    _preferences.end();
}

/**
 * @brief Loads all persistent data items from NVS into member variables.
 */
void API::_loadPersistentData() {
    _accessToken = _preferences.getString(KEY_ACCESS_TOKEN, "");
    _refreshToken = _preferences.getString(KEY_REFRESH_TOKEN, "");
    _dataCollectionTimeMinutes = _preferences.getInt(KEY_DATA_COLLECTION_TIME, DEFAULT_DATA_COLLECTION_MINUTES);
    _activatedFlag = _preferences.getBool(KEY_IS_ACTIVATED, false);

    // Ensure _dataCollectionTimeMinutes has a sane default if NVS was empty or had 0
    if (_dataCollectionTimeMinutes <= 0) {
        _dataCollectionTimeMinutes = DEFAULT_DATA_COLLECTION_MINUTES;
    }
}

// --- NVS Save Methods ---
void API::_saveAccessToken(const String& token) {
    _accessToken = token;
    _preferences.putString(KEY_ACCESS_TOKEN, _accessToken);
}

void API::_saveRefreshToken(const String& token) {
    _refreshToken = token;
    _preferences.putString(KEY_REFRESH_TOKEN, _refreshToken);
}

void API::_saveDataCollectionTime(int minutes) {
    if (minutes <= 0) minutes = DEFAULT_DATA_COLLECTION_MINUTES; // Ensure sane value
    _dataCollectionTimeMinutes = minutes;
    _preferences.putInt(KEY_DATA_COLLECTION_TIME, _dataCollectionTimeMinutes);
}

void API::_saveActivationStatus(bool activated) {
    _activatedFlag = activated;
    _preferences.putBool(KEY_IS_ACTIVATED, _activatedFlag);
}

// --- Public Getter Methods ---
bool API::isActivated() const {
    return _activatedFlag;
}

String API::getAccessToken() const {
    return _accessToken;
}

String API::getBaseApiUrl() const {
    return _apiBaseUrl;
}

int API::getDataCollectionTimeMinutes() const {
    return _dataCollectionTimeMinutes > 0 ? _dataCollectionTimeMinutes : DEFAULT_DATA_COLLECTION_MINUTES;
}

// --- Core API Interaction Methods ---

/**
 * @brief Performs an HTTP POST request.
 */
int API::_httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload) {
    HTTPClient http;
    int httpResponseCode = -1; // Default to client error

    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_httpPost] No WiFi connection.");
        #endif
        return -100; // Custom error code for no WiFi
    }

    if (http.begin(fullUrl)) {
        http.setTimeout(HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        if (!authorizationToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + authorizationToken);
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_httpPost] POST to: %s\n", fullUrl.c_str());
            if(!jsonPayload.isEmpty()) Serial.printf("[API_httpPost] Payload: %s\n", jsonPayload.c_str());
            if(!authorizationToken.isEmpty()) Serial.printf("[API_httpPost] Auth: Device %s\n", authorizationToken.substring(0,10).c_str()); // Log first 10 chars of token
        #endif

        if(jsonPayload.isEmpty()){ // For requests like /auth that might not have a body but are POST
             httpResponseCode = http.POST("");
        } else {
             httpResponseCode = http.POST(jsonPayload);
        }


        if (httpResponseCode > 0) {
            responsePayload = http.getString();
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_httpPost] Response Code: %d\n", httpResponseCode);
                Serial.printf("[API_httpPost] Response Payload: %s\n", responsePayload.c_str());
            #endif
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_httpPost] Request failed, error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            #endif
        }
        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_httpPost] Unable to begin connection to %s\n", fullUrl.c_str());
        #endif
        httpResponseCode = -101; // Custom error code for connection begin failed
    }
    return httpResponseCode;
}

/**
 * @brief Parses DeviceActivationResponseDto or DeviceAuthResponseDto.
 */
bool API::_parseAndStoreAuthResponse(const String& jsonResponse) {
    if (jsonResponse.isEmpty()) {
        return false;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonResponse);

    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(F("[API_parse] JSON deserialization failed: "));
            Serial.println(error.f_str());
        #endif
        return false;
    }

    // Extract fields - assuming DTOs have these exact names
    // {"accessToken":"...", "refreshToken":"...", "dataCollectionTimeMinutes":...}
    const char* newAccessToken = doc["accessToken"];
    const char* newRefreshToken = doc["refreshToken"];
    int newDataCollectionTime = doc["dataCollectionTime"] | 0; // Default to 0 if not present or invalid

    bool success = false;
    if (newAccessToken && newRefreshToken) { // Both tokens must be present
        if (String(newAccessToken) != _accessToken) {
            _saveAccessToken(String(newAccessToken));
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[API_parse] AccessToken updated."));
            #endif
        }
        if (String(newRefreshToken) != _refreshToken) {
            _saveRefreshToken(String(newRefreshToken));
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[API_parse] RefreshToken updated."));
            #endif
        }
        success = true; // Tokens processed
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_parse] accessToken or refreshToken missing in JSON response."));
        #endif
        return false; // Essential tokens missing
    }
    
    if (newDataCollectionTime > 0 && newDataCollectionTime != _dataCollectionTimeMinutes) {
        _saveDataCollectionTime(newDataCollectionTime);
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_parse] DataCollectionTimeMinutes updated to: %d\n", newDataCollectionTime);
        #endif
    } else if (doc.containsKey("dataCollectionTimeMinutes") && newDataCollectionTime <= 0) {
        // If the key exists but the value is invalid (e.g. 0 or negative), it might be an issue.
        // For now, we keep the existing or default. Could log a warning.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_parse] Received invalid dataCollectionTimeMinutes: %d. Keeping current: %d\n", newDataCollectionTime, _dataCollectionTimeMinutes);
        #endif
    }


    return success;
}

/**
 * @brief Attempts device activation.
 */
int API::performActivation(const String& deviceId, const String& activationCode) {
    if (deviceId.isEmpty() || activationCode.isEmpty()) {
        return -102; // Custom error for missing params
    }

    JsonDocument doc;
    doc["deviceId"] = deviceId.toInt(); // As per user story, DeviceId is int
    doc["activationCode"] = activationCode;

    String payload;
    serializeJson(doc, payload);
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiActivatePath;

    int httpCode = _httpPost(fullUrl, "", payload, responsePayload); // No auth token for activation

    if (httpCode == 200) {
        if (_parseAndStoreAuthResponse(responsePayload)) {
            _saveActivationStatus(true);
        } else {
            // Successful HTTP but failed to parse or missing tokens in response
            httpCode = -103; // Custom error for parse failure
            _saveActivationStatus(false); // Ensure not marked as active
        }
    } else {
        _saveActivationStatus(false); // Activation failed, ensure not marked as active
    }
    return httpCode;
}

/**
 * @brief Checks backend and authentication.
 */
int API::checkBackendAndAuth() {
    if (!_activatedFlag) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Device not activated. Skipping auth check."));
        #endif
        return -104; // Custom error for not activated
    }
    if (_accessToken.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] No access token available. Attempting refresh first."));
        #endif
        // If no access token, try to refresh first (e.g. after a restart with valid refresh token)
        int refreshHttpCode = performTokenRefresh();
        if(refreshHttpCode != 200) {
             // Refresh failed, cannot proceed with auth check using an access token
            return refreshHttpCode; // Return the error code from refresh attempt
        }
        // If refresh was successful, _accessToken is now populated. Proceed.
    }

    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiAuthPath;

    // Create the JSON payload for the /auth request
    JsonDocument docAuthRequest;
    docAuthRequest["token"] = _accessToken; // Use the current access token
    String authRequestPayload;
    serializeJson(docAuthRequest, authRequestPayload);

    if (authRequestPayload.isEmpty() && !_accessToken.isEmpty()) { // Check if serialization failed but token was present
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Failed to serialize auth request payload for /auth endpoint."));
        #endif
        // Return a custom error code indicating payload serialization failure for a non-empty token
        // to distinguish from an empty token case if that were handled differently.
        return -106; 
    }
    
    // If _accessToken was empty and performTokenRefresh populated it, 
    // authRequestPayload would have been built with the new token.
    // If _accessToken was initially present, authRequestPayload uses that.
    // The _httpPost method requires an accessToken for its header, which should be the same one used in the payload.
    int httpCode = _httpPost(fullUrl, _accessToken, authRequestPayload, responsePayload);

    if (httpCode == 200) {
        _parseAndStoreAuthResponse(responsePayload); // Update tokens/collection time if different
    } else if (httpCode == 401) { // Unauthorized, token likely expired or invalid
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Auth endpoint returned 401. Attempting token refresh."));
        #endif
        httpCode = performTokenRefresh(); // This will handle deactivation on critical refresh failure
    }
    // Other HTTP codes (e.g., 500, 400, network errors) are returned as is.
    return httpCode;
}

/**
 * @brief Attempts token refresh.
 */
int API::performTokenRefresh() {
    if (_refreshToken.isEmpty()) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] No refresh token available. Cannot refresh."));
        #endif
        _saveActivationStatus(false); // Cannot refresh, so consider deactivated
        return -105; // Custom error for no refresh token
    }

    JsonDocument doc;
    doc["token"] = _refreshToken; // As per user story: {"token": stored_refresh_token}

    String payload;
    serializeJson(doc, payload);
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiRefreshTokenPath;

    // Assuming refresh token endpoint does not need the (potentially expired) access token for Auth header
    int httpCode = _httpPost(fullUrl, "", payload, responsePayload);

    if (httpCode == 200) {
        if (!_parseAndStoreAuthResponse(responsePayload)) {
            // Successful HTTP but failed to parse or missing tokens in response
            httpCode = -103; // Custom error for parse failure
            // Do not necessarily deactivate here, as old tokens might still be somewhat valid
            // until explicitly invalidated by a 401 on refresh itself.
        }
    } else if (httpCode == 401) { // Refresh token itself is invalid/expired
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] Refresh token rejected (401). Deactivating device."));
        #endif
        _saveAccessToken("");    // Clear tokens
        _saveRefreshToken("");
        _saveActivationStatus(false); // Deactivate
    }
    // Other HTTP codes are returned as is.
    return httpCode;
}