/**
 * @file API.h
 * @brief Defines the API class for handling device activation, backend communication,
 * and storing relevant data in Non-Volatile Storage (NVS).
 * @note This class interacts with a backend API over HTTPS.
 */
#ifndef API_H
#define API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "LEDStatus.h" // Assuming LEDStatus.h defines the necessary states like ERROR_AUTH, ERROR_SEND, OFF

// Backend Endpoints (HTTPS is used)
#define ACTIVATION_ENDPOINT       "https://backend.example.com/activate"        ///< Endpoint for device activation requests.
#define CHECK_ACTIVATION_ENDPOINT "https://backend.example.com/check_activation/" ///< Endpoint for checking device activation status (expects device ID appended).
#define BACKEND_STATUS_ENDPOINT   "https://backend.example.com/status"          ///< Endpoint for checking backend health/status.

// --- Security Credentials ---
// @warning These credentials (JWT, Device ID) are hardcoded.
//          For production environments, consider more secure methods like provisioning or secure storage.
#define ACTIVATION_JWT "your_activation_jwt_here" ///< Activation JSON Web Token.
#define DEVICE_ID      "your_device_id_here"      ///< Unique Device Identifier.

// Activation States stored in NVS
#define STATE_ACTIVATED     "ACTIVATED"     ///< String identifier for the activated state.
#define STATE_NO_ACTIVATED  "NO_ACTIVATED"  ///< String identifier for the non-activated state.

// Keys for storing data in Preferences (NVS)
#define PREF_ACTIVATION_STATE "activation_state" ///< NVS key for the activation state.
#define PREF_AUTH_TOKEN       "auth_token"       ///< NVS key for the authentication token.

// HTTP Request Configuration
#define HTTP_TIMEOUT     10000 ///< HTTP request timeout in milliseconds (10 seconds).
#define HTTP_MAX_RETRIES 3     ///< Maximum number of retry attempts for HTTP requests.

/**
 * @class API
 * @brief Manages device activation, authentication token retrieval, and backend status checks via HTTPS.
 *
 * This class handles the communication with the backend API for critical operations
 * like activating the device upon first use and periodically checking its status
 * and retrieving authentication tokens. It uses the Preferences library to persist
 * the activation state and the latest auth token in Non-Volatile Storage (NVS).
 * It also uses an LEDStatus object to provide visual feedback on operation success or failure.
 * @note This class might be under development or requires specific backend infrastructure.
 */
class API {
public:
    /**
     * @brief Constructor for the API class.
     * Initializes the Preferences library namespace and loads the auth token from NVS if available.
     */
    API();

    /**
     * @brief Attempts to activate the device with the backend.
     * Sends the predefined activation JWT to the activation endpoint.
     * If the backend responds with HTTP 200 OK, it saves the "ACTIVATED" state to NVS.
     * Otherwise, it indicates an authentication error via LED and saves "NO_ACTIVATED" state.
     * Includes retry logic defined by HTTP_MAX_RETRIES.
     * @return true if activation was successful within retry limits, false otherwise.
     */
    bool activateDevice();

    /**
     * @brief Verifies if the device is still considered active by the backend and retrieves the auth token.
     * Sends the device ID to the check activation endpoint. Expects an HTTP 200 OK response
     * containing a JSON payload with an authentication token (e.g., {"token": "new_token"}).
     * - If a new token is received (different from the currently stored one), it's updated
     * both in memory and in NVS.
     * - If the token is the same, no update occurs.
     * If the response is not 200 OK or the token is missing/invalid, it indicates an
     * authentication error via LED and saves the "NO_ACTIVATED" state.
     * Includes retry logic.
     * @return true if the device is confirmed active and token handling was successful, false otherwise.
     */
    bool checkActivation();

    /**
     * @brief Performs a simple check to see if the backend API is online and reachable.
     * Makes a GET request to the backend status endpoint.
     * Considers the backend online if it receives an HTTP 200 OK response.
     * (Optionally, could be extended to check the response body, e.g., {"status":"OK"}).
     * Indicates a communication error via LED on failure.
     * @return true if the backend responded successfully (HTTP 200), false otherwise.
     */
    bool checkBackendStatus();

    /**
     * @brief Retrieves the currently stored authentication token.
     * Returns the token held in the volatile memory (_authToken).
     * @return String containing the current authentication token. Might be empty if none is loaded or retrieved.
     */
    String getAuthToken();

private:
    LEDStatus _led;          ///< LEDStatus object instance for visual feedback.
    Preferences _preferences;///< Preferences object instance for NVS access.
    String _authToken;       ///< In-memory copy of the authentication token.

    /**
     * @brief Saves the given activation state string to NVS.
     * Uses the PREF_ACTIVATION_STATE key.
     * @param state The activation state string to save (e.g., STATE_ACTIVATED or STATE_NO_ACTIVATED).
     */
    void saveActivationState(const char* state);

    /**
     * @brief Saves the given authentication token to NVS and updates the in-memory copy.
     * Uses the PREF_AUTH_TOKEN key.
     * @param token The authentication token string to save.
     */
    void saveAuthToken(const String &token);
};

#endif // API_H