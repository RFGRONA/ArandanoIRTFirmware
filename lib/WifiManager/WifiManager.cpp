/**
 * @file WiFiManager.cpp
 * @brief Implements the WiFiManager class for handling WiFi connection, state, and reconnection logic on ESP32.
 */
#include "WiFiManager.h"
#include <HTTPClient.h> // Ensure this is included (needed by checkInternetConnection)

// Initialize the static singleton pointer to null. Must be done outside the class definition.
WiFiManager* WiFiManager::_instance = nullptr;

/**
 * @brief Constructor implementation.
 */
WiFiManager::WiFiManager(): // Use initializer list for member variables
    _lastReconnectAttempt(0),
    _reconnectAttempts(0),
    _currentStatus(DISCONNECTED),
    _ssid(""), // Initialize credentials to empty strings
    _password("")
    // Note: _led is default constructed here
{
    // --- Singleton Pattern Implementation ---
    // Store the 'this' pointer in the static member '_instance'.
    // This allows the static event handler functions to access the methods and members
    // of the single WiFiManager object created in the sketch.
    // Basic check to prevent accidental overwriting if multiple instances were created (though design assumes one).
    if (_instance == nullptr) {
        _instance = this;
    } else {
        // Optional: Log a warning if multiple instances are detected.
        // Serial.println("Warning: Multiple WiFiManager instances created. Static callbacks may not work as expected.");
    }

    // --- Register WiFi Event Handlers ---
    // It's good practice to remove any potentially existing handlers before adding new ones,
    // especially if the object might be re-initialized or if other code could manipulate events.
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Register our static member functions as callbacks for specific WiFi system events.
    // The ESP WiFi library will call these static functions when the corresponding event occurs.
    WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED); // Connected to AP
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);         // Got IP Address
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED); // Disconnected from AP
}

/**
 * @brief Basic WiFi initialization implementation.
 */
bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);          // Set ESP32 WiFi mode to Station (client).
    WiFi.setAutoReconnect(false); // IMPORTANT: Disable the built-in auto-reconnect mechanism.
                                  // We handle reconnection manually in handleWiFi() for more control.
    WiFi.disconnect(false);       // Ensure WiFi is in a disconnected state initially, without erasing saved config.
    delay(100);                   // Short delay to allow hardware/driver to settle.
    return true;                  // Indicate basic setup success.
}

/**
 * @brief Initiates connection to the configured WiFi network implementation.
 */
bool WiFiManager::connectToWiFi() {
    // Do not start a new connection if already connected or currently connecting.
    if (_currentStatus == CONNECTED || _currentStatus == CONNECTING) {
        #ifdef ENABLE_DEBUG_SERIAL // Use conditional compilation for debug messages
            if (_currentStatus == CONNECTED) Serial.println("WiFiManager: Already connected.");
            if (_currentStatus == CONNECTING) Serial.println("WiFiManager: Connection already in progress.");
        #endif
        // Return value indicates if *already* connected, not if a new attempt started.
        return _currentStatus == CONNECTED; // Return true if connected, false if still connecting
    }

    // Proceed with initiating a new connection attempt.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFiManager: Initiating connection to SSID: " + _ssid);
    #endif
    _currentStatus = CONNECTING;         // Update internal state.
    _led.setState(CONNECTING_WIFI);      // Update visual indicator.
    _reconnectAttempts = 0;              // Reset automatic retry counter for this new manual attempt.
    _lastReconnectAttempt = millis();    // Record start time for connection timeout tracking.

    // Ensure WiFi is disconnected before starting a new connection attempt.
    WiFi.disconnect(false);
    delay(100);                          // Short delay.

    // Start the asynchronous connection process using stored credentials.
    // The actual connection success/failure will be handled by the event callbacks.
    WiFi.begin(_ssid.c_str(), _password.c_str());

    return true; // Indicate that a new connection attempt was initiated.
}

/**
 * @brief Forces disconnection implementation.
 */
void WiFiManager::disconnect() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFiManager: Forcing disconnection.");
    #endif
    WiFi.disconnect(false);             // Tell the ESP32 to disconnect.
    _currentStatus = DISCONNECTED;      // Set internal state immediately.
    // Consider a specific LED state for forced disconnect vs. connection lost/failed?
    _led.setState(ERROR_WIFI);          // Update visual indicator (using generic error for now).
}

/**
 * @brief Internal helper method to attempt reconnection implementation.
 */
void WiFiManager::attemptReconnect() {
    // Only proceed if reconnection is warranted and not already in progress/connected.
    if (_currentStatus == CONNECTING || _currentStatus == CONNECTED) {
        return; // Do nothing if already trying or succeeded.
    }

    // Check if the configured interval has passed since the last attempt or disconnection event.
    if (millis() - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {

        // Check if we haven't exceeded the maximum allowed automatic reconnection attempts.
        if (_reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
            _reconnectAttempts++; // Increment the counter for this attempt.
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("WiFiManager: Attempting reconnect #%d to SSID: %s\n", _reconnectAttempts, _ssid.c_str());
            #endif
            _currentStatus = CONNECTING;         // Set state to indicate connection is in progress.
            _led.setState(CONNECTING_WIFI);      // Update visual indicator.
            _lastReconnectAttempt = millis();    // Record the time of *this* attempt for timeout checking.

            // Initiate the reconnection attempt.
            WiFi.disconnect(false); // Ensure clean state before reconnecting.
            delay(100);
            WiFi.begin(_ssid.c_str(), _password.c_str()); // Start connection attempt.

        } else {
            // Maximum reconnection attempts have been reached.
            // Transition to a permanent failure state until manually reset/restarted.
            if (_currentStatus != CONNECTION_FAILED) { // Only log the transition once
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("WiFiManager Error: Maximum reconnect attempts reached. Stopping automatic retries.");
                 #endif
                 _currentStatus = CONNECTION_FAILED; // Set state to permanent failure.
                 _led.setState(ERROR_WIFI);         // Update visual indicator.
            }
        }
    }
}

/**
 * @brief Main state handling loop implementation.
 */
void WiFiManager::handleWiFi() {
    // Check if we need to attempt reconnection based on current state.
    // Includes DISCONNECTED, CONNECTION_LOST, and CONNECTION_FAILED (if retries are not exhausted).
    if (_currentStatus == DISCONNECTED || _currentStatus == CONNECTION_LOST ||
       (_currentStatus == CONNECTION_FAILED && _reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS))
    {
        attemptReconnect();
    }

    // Check for connection timeout specifically when in the CONNECTING state.
    if (_currentStatus == CONNECTING && (millis() - _lastReconnectAttempt > WIFI_CONNECT_TIMEOUT)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("WiFiManager Warning: Connection attempt timed out.");
        #endif
        WiFi.disconnect(false); // Stop the timed-out connection attempt.

        // Determine the next state based on whether max retries were already hit during this attempt.
        // If retries are exhausted, move to FAILED; otherwise, consider it LOST and allow retries.
        _currentStatus = (_reconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS) ? CONNECTION_FAILED : CONNECTION_LOST;
        _led.setState(ERROR_WIFI);         // Set LED to error state.
        _lastReconnectAttempt = millis();    // Update timestamp to respect the reconnect interval for the *next* attempt.
        // If it failed due to timeout on the *last* allowed attempt, the next call to attemptReconnect() will move it to CONNECTION_FAILED.
    }
}

/**
 * @brief Getter for connection status implementation.
 */
WiFiManager::ConnectionStatus WiFiManager::getConnectionStatus() {
    return _currentStatus;
}

// --- Static Callback Implementations ---

/**
 * @brief Static callback for WiFi Connected event implementation.
 */
/* static */ void WiFiManager::onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    // This event just means association/authentication with the AP succeeded.
    // We typically wait for the 'GOT_IP' event before considering the connection usable.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFi Event: Station Connected to AP.");
    #endif
    // No state change needed here usually via _instance.
}

/**
 * @brief Static callback for WiFi Got IP event implementation.
 */
/* static */ void WiFiManager::onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    // Check if the singleton instance pointer is valid before using it.
    if (_instance != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("WiFi Event: Station Got IP: ");
            Serial.println(WiFi.localIP()); // Log the obtained IP address
        #endif
        _instance->_currentStatus = CONNECTED;      // Update the state to fully connected.
        _instance->_reconnectAttempts = 0;          // Reset the retry counter on success.
        _instance->_led.setState(ALL_OK);           // Update LED to indicate success/idle.
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("WiFiManager Error: onWiFiGotIP called but _instance is null!");
         #endif
    }
}

/**
 * @brief Static callback for WiFi Disconnected event implementation.
 */
/* static */ void WiFiManager::onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    // Check if the singleton instance pointer is valid.
    if (_instance != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Log the disconnection event and the reason code provided by the system.
            Serial.printf("WiFi Event: Station Disconnected. Reason: %d\n", info.wifi_sta_disconnected.reason);
            // You can look up reason codes in ESP-IDF documentation (enum wifi_err_reason_t)
            // e.g., 2=AUTH_EXPIRE, 7=AUTH_LEAVE, 8=ASSOC_LEAVE, 200=BEACON_TIMEOUT, 201=NO_AP_FOUND etc.
        #endif

        // Only transition to CONNECTION_LOST if we were previously fully CONNECTED.
        // Avoid overriding CONNECTING or CONNECTION_FAILED states here, as handleWiFi manages those transitions.
        if (_instance->_currentStatus == CONNECTED) {
            _instance->_currentStatus = CONNECTION_LOST; // Mark that the established connection was dropped.
            _instance->_led.setState(ERROR_WIFI);       // Update visual indicator.
            _instance->_lastReconnectAttempt = millis(); // Record the time of disconnection to time the next retry.
        } else if (_instance->_currentStatus == CONNECTING) {
             #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("WiFiManager: Disconnected while in CONNECTING state (could be auth failure, timeout, etc.). handleWiFi will manage next steps.");
             #endif
            // Let the timeout logic in handleWiFi or subsequent calls to attemptReconnect manage the state.
        }
        // If already DISCONNECTED or CONNECTION_FAILED, no state change needed here.
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("WiFiManager Error: onWiFiDisconnected called but _instance is null!");
         #endif
    }
}

/**
 * @brief Checks internet connectivity implementation.
 */
bool WiFiManager::checkInternetConnection(const String& testUrl) {
    // Pre-check: Must be in CONNECTED state according to our manager *and* ESP WiFi library.
    if (_currentStatus != CONNECTED || WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            if (_currentStatus != CONNECTED) Serial.println("Internet Check: Skipped, WiFiManager status is not CONNECTED.");
            if (WiFi.status() != WL_CONNECTED) Serial.println("Internet Check: Skipped, WiFi.status() is not WL_CONNECTED.");
        #endif
        return false;
    }

    HTTPClient http;
    // Set a connection timeout for the HTTP request (milliseconds)
    http.setConnectTimeout(5000); // Example: 5 seconds
    bool internetAvailable = false; // Assume no connection initially

    // Use 'if (http.begin(...))' to handle potential errors in URL parsing or client setup.
    if (http.begin(testUrl)) {
        // Send the HTTP GET request.
        int httpCode = http.GET();

        // Check if the response code indicates success (200 OK or 204 No Content are typical for connectivity checks).
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT) {
            internetAvailable = true;
            #ifdef ENABLE_DEBUG_SERIAL
                // Serial.printf("Internet Check: Success (HTTP %d)\n", httpCode);
            #endif
        } else {
             #ifdef ENABLE_DEBUG_SERIAL
                if (httpCode > 0) {
                     Serial.printf("Internet Check: Failed, server returned HTTP code: %d\n", httpCode);
                } else {
                     Serial.printf("Internet Check: Failed, HTTP GET error: %s\n", http.errorToString(httpCode).c_str());
                }
            #endif
            internetAvailable = false;
        }

        // Release the HTTP client resources.
        http.end();
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Internet Check: Failed, http.begin() returned false for URL: " + testUrl);
         #endif
        internetAvailable = false;
    }

    return internetAvailable;
}

/**
 * @brief Sets WiFi credentials implementation.
 */
void WiFiManager::setCredentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
     #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFiManager: Credentials set for SSID: " + _ssid);
     #endif
}