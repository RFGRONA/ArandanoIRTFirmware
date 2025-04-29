/**
 * @file API.cpp
 * @brief Implements the API class methods for backend communication and NVS management.
 */
#include "API.h"

/**
 * @brief Constructor implementation.
 */
API::API() {
    // Initialize the Preferences library within the "activation" namespace.
    // The second argument 'false' means read/write mode.
    _preferences.begin("activation", false);
    // Load the previously saved authentication token from NVS, if it exists.
    // If not found, initialize _authToken to an empty string.
    _authToken = _preferences.getString(PREF_AUTH_TOKEN, "");
}

/**
 * @brief Saves the activation state to NVS.
 */
void API::saveActivationState(const char* state) {
    _preferences.putString(PREF_ACTIVATION_STATE, state);
}

/**
 * @brief Saves the authentication token to NVS and updates the in-memory variable.
 */
void API::saveAuthToken(const String &token) {
    _preferences.putString(PREF_AUTH_TOKEN, token);
    _authToken = token; // Keep the in-memory copy synchronized
}

/**
 * @brief Implementation of the device activation logic.
 */
bool API::activateDevice() {
    int attempt = 0;
    int httpResponseCode = 0;
    bool activated = false;

    // Retry the activation process up to HTTP_MAX_RETRIES times
    while (attempt < HTTP_MAX_RETRIES && !activated) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);
        // @note Ensure the root CA certificate for the server is available
        //       to the ESP32's WiFiClientSecure layer if using HTTPS.
        //       This might involve http.begin(url, root_ca_certificate);
        http.begin(ACTIVATION_ENDPOINT);
        http.addHeader("Content-Type", "application/json");

        // Construct JSON payload with the activation JWT
        JsonDocument doc; // Use JsonDocument for ESP32 ArduinoJson v6+
        doc["activation_code"] = ACTIVATION_JWT;
        String payload;
        serializeJson(doc, payload);

        // Perform the POST request
        httpResponseCode = http.POST(payload);
        http.end(); // Release resources

        if (httpResponseCode == 200) {
            // Activation successful: save state and exit loop
            saveActivationState(STATE_ACTIVATED);
            activated = true;
        } else {
            // Activation failed on this attempt
            attempt++;
            // Optional: Log the error code: Serial.printf("[API] Activation failed, HTTP code: %d\n", httpResponseCode);
            delay(500); // Short delay before retrying
        }
    }

    // If activation failed after all retries
    if (!activated) {
        // Indicate error state visually
        _led.setState(ERROR_AUTH); // Assuming ERROR_AUTH is defined in LEDStatus.h
        delay(5000); // Show error state for a few seconds
        _led.setState(OFF);  // Turn LED off
        // Save non-activated state to NVS
        saveActivationState(STATE_NO_ACTIVATED);
        // Optional: Log the final failure: Serial.println("[API] Device activation failed permanently.");
    }

    return activated;
}

/**
 * @brief Implementation of the activation check and token retrieval logic.
 */
bool API::checkActivation() {
    int attempt = 0;
    int httpResponseCode = 0;
    bool active = false;

    // Retry the check process up to HTTP_MAX_RETRIES times
    while (attempt < HTTP_MAX_RETRIES && !active) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);

        // Construct the URL by appending the device ID
        String url = String(CHECK_ACTIVATION_ENDPOINT) + DEVICE_ID;
        // @note As above, ensure HTTPS certificate handling if needed.
        http.begin(url);
        // Add the current auth token (or activation JWT initially?) to the Authorization header
        // @note The current code uses ACTIVATION_JWT. Depending on the backend API design,
        //       this might need to be `_authToken` after the first successful checkActivation
        //       or if activation and subsequent checks use different tokens. Revisit this logic.
        http.addHeader("Authorization", "Bearer " + String(ACTIVATION_JWT)); // Assuming Bearer token auth

        // Perform the GET request
        httpResponseCode = http.GET();

        if (httpResponseCode == 200) {
            // Got a successful response, try to parse the JSON payload
            String response = http.getString();
            http.end(); // End connection now that we have the response

            JsonDocument responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);

            if (!error) {
                // Successfully parsed JSON, look for the token
                const char* newTokenCStr = responseDoc["token"]; // Assuming the key is "token"
                if (newTokenCStr != nullptr) {
                    String receivedToken(newTokenCStr);
                    // Check if the received token is different from the current one
                    if (receivedToken != _authToken) {
                         // Optional: Log token update: Serial.println("[API] Auth token updated.");
                        saveAuthToken(receivedToken); // Save new token to NVS and memory
                    } else {
                        // Optional: Log token unchanged: Serial.println("[API] Auth token verified, no change.");
                    }
                    active = true; // Mark as active and exit loop
                    break; // Exit the while loop successfully
                } else {
                     // Optional: Log token missing: Serial.println("[API] Token key missing in JSON response.");
                }
            } else {
                // Optional: Log JSON error: Serial.printf("[API] JSON deserialization failed: %s\n", error.c_str());
            }
        } else {
            // HTTP request failed
            http.end(); // Ensure connection is closed on error too
            // Optional: Log HTTP error: Serial.printf("[API] Check activation failed, HTTP code: %d\n", httpResponseCode);
        }

        // Increment attempt counter and wait before retrying
        attempt++;
        delay(500);
    }

    // If the check failed after all retries
    if (!active) {
        // Indicate error state visually
        _led.setState(ERROR_AUTH);
        delay(5000); // Show error state
        _led.setState(OFF);
        // Save non-activated state as the check failed
        saveActivationState(STATE_NO_ACTIVATED);
        // Optional: Log final failure: Serial.println("[API] Device activation check failed permanently.");
    }

    return active;
}

/**
 * @brief Implementation of the backend status check logic.
 */
bool API::checkBackendStatus() {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT);
    // @note As above, ensure HTTPS certificate handling if needed.
    http.begin(BACKEND_STATUS_ENDPOINT);

    // Perform a simple GET request
    int httpResponseCode = http.GET();
    http.end(); // Release resources

    if (httpResponseCode != 200) {
        // Backend seems unavailable or returned an error
        _led.setState(ERROR_SEND); // Assuming ERROR_SEND indicates communication issues
        delay(5000); // Show error
        _led.setState(OFF);
        // Optional: Log the failure: Serial.printf("[API] Backend status check failed, HTTP code: %d\n", httpResponseCode);
        return false;
    }

    // Backend responded with 200 OK
    // Optional: Log success: Serial.println("[API] Backend status check successful (HTTP 200).");
    return true;
}

/**
 * @brief Returns the current authentication token stored in memory.
 */
String API::getAuthToken() {
    return _authToken;
}