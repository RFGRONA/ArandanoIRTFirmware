#include "WiFiManager.h"
#include <HTTPClient.h> // Ensure this is included (needed for checkInternetConnection)

// Initialize the static singleton pointer to null.
WiFiManager* WiFiManager::_instance = nullptr;

// Constructor: Stores credentials, initializes state, sets up the singleton instance,
// and registers static callback functions for WiFi events.
WiFiManager::WiFiManager(): // Initialize internal state variables
    _reconnectAttempts(0),
    _currentStatus(DISCONNECTED),
    _ssid(""),      
    _password("")
{ 

    // Singleton pattern: Store the instance pointer for static callbacks.
    // Assumes only one WiFiManager object will be created.
    if (_instance == nullptr) {
        _instance = this;
    } // else: Warning about multiple instances was removed in original code.

    // Ensure previous event handlers are detached before attaching new ones
    // (Good practice if multiple managers could exist or if re-initialized).
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Register static member functions as callbacks for WiFi system events.
    WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

// Basic WiFi initialization: set mode to Station and disable built-in auto-reconnect.
bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);          // Set WiFi mode to Station
    WiFi.setAutoReconnect(false); // Disable ESP-IDF's auto-reconnect; we handle it manually
    WiFi.disconnect(false);       // Ensure disconnected initially without erasing config
    delay(100);                   // Short delay
    return true;
}

// Starts the connection process if not already connected or connecting.
bool WiFiManager::connectToWiFi() {
    // Prevent starting a new connection if already connected or trying to connect.
    if (_currentStatus == CONNECTED || _currentStatus == CONNECTING) {
         return _currentStatus == CONNECTED; // Return true if already connected, false if connecting
    }

    // Update state and visual indicator
    _currentStatus = CONNECTING;
    _led.setState(CONNECTING_WIFI); // Update LED status
    _reconnectAttempts = 0;         // Reset attempt counter for a manual connection attempt
    _lastReconnectAttempt = millis(); // Record start time for timeout tracking

    // Ensure disconnected state before beginning connection attempt
    WiFi.disconnect(false);
    delay(100);
    // Initiate the connection using stored credentials
    WiFi.begin(_ssid.c_str(), _password.c_str());

    return true; // Indicate that the connection attempt has started
}

// Forces disconnection and updates state.
void WiFiManager::disconnect() {
    WiFi.disconnect(false); // Disconnect from the network
    _currentStatus = DISCONNECTED; // Set state
    _led.setState(ERROR_WIFI);    // Update LED (perhaps a specific DISCONNECTED color?)
}

// Attempts to reconnect if disconnected/lost and conditions are met.
void WiFiManager::attemptReconnect() {
    // Only attempt if not already connecting or connected.
    if (_currentStatus == CONNECTING || _currentStatus == CONNECTED) return;

    // Check if enough time has passed since the last attempt/disconnection.
    if (millis() - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
        // Check if the maximum number of retries has been exceeded.
        if (_reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
            _reconnectAttempts++; // Increment retry counter
            _currentStatus = CONNECTING; // Set state to connecting
            _led.setState(CONNECTING_WIFI); // Update LED
            _lastReconnectAttempt = millis(); // Record time of this attempt

            // Perform the reconnection attempt
            WiFi.disconnect(false); // Ensure clean state
            delay(100);
            WiFi.begin(_ssid.c_str(), _password.c_str());
            // Serial.printf("WiFi reconnect attempt #%d\n", _reconnectAttempts); // Optional Debug
        } else {
             // Max retries reached, set status to failed.
             _currentStatus = CONNECTION_FAILED;
             _led.setState(ERROR_WIFI);
             // Serial.println("WiFi max reconnect attempts reached."); // Optional Debug
        }
    }
}

// Main state handling function, intended to be called in the loop.
void WiFiManager::handleWiFi() {
    // If disconnected, connection lost, or failed (but not permanently failed), try reconnecting.
    if (_currentStatus == DISCONNECTED || _currentStatus == CONNECTION_LOST ||
        (_currentStatus == CONNECTION_FAILED && _reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS)) {
       attemptReconnect();
    }

    // Check for connection timeout if currently in the CONNECTING state.
    if (_currentStatus == CONNECTING && (millis() - _lastReconnectAttempt > WIFI_CONNECT_TIMEOUT)) {
        // Serial.println("WiFi connection attempt timed out."); // Optional Debug
        WiFi.disconnect(false); // Stop the connection attempt
        // Decide if it's a temporary loss or permanent failure based on retries
        _currentStatus = (_reconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS) ? CONNECTION_FAILED : CONNECTION_LOST;
        _led.setState(ERROR_WIFI); // Set LED to error state
        _lastReconnectAttempt = millis(); // Update timestamp to prevent immediate retry
    }
}

// Simple getter for the current connection status.
WiFiManager::ConnectionStatus WiFiManager::getConnectionStatus() {
    return _currentStatus;
}

// --- Static Callback Implementations ---
// These are called by the WiFi event system. They use the singleton '_instance'.

// Called when the ESP32 connects to the Access Point (authentication successful).
void WiFiManager::onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    // Typically, we wait for the IP address before considering the connection fully established.
    // So, this callback might not need to change the state immediately.
    // Serial.println("WiFi Event: STA Connected"); // Optional Debug
}

// Called when the ESP32 obtains an IP address from the DHCP server.
void WiFiManager::onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance != nullptr) { // Check if the instance exists
        // Serial.print("WiFi Event: STA Got IP: "); // Optional Debug
        // Serial.println(WiFi.localIP()); // Optional Debug
        _instance->_currentStatus = CONNECTED; // Update state to fully connected
        _instance->_reconnectAttempts = 0;     // Reset retry counter on successful connection
        _instance->_led.setState(ALL_OK);    // Update LED status
    }
}

// Called when the ESP32 disconnects from the Access Point.
void WiFiManager::onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance != nullptr) { // Check if the instance exists
        // Serial.printf("WiFi Event: STA Disconnected, reason: %d\n", info.wifi_sta_disconnected.reason); // Optional Debug
        // Only change state if we were previously connected (avoid changing from CONNECTING or FAILED).
        if (_instance->_currentStatus == CONNECTED) {
             _instance->_currentStatus = CONNECTION_LOST; // Mark connection as lost
             _instance->_led.setState(ERROR_WIFI);      // Update LED status
             _instance->_lastReconnectAttempt = millis(); // Record time for reconnection logic
        } else if (_instance->_currentStatus == CONNECTING) {
            // If disconnected while trying to connect, could indicate password error etc.
            // Let the handleWiFi timeout or attemptReconnect logic handle state change.
        }
    }
}

// Checks for basic internet connectivity via an HTTP GET request.
bool WiFiManager::checkInternetConnection(const String& testUrl) {
    // Can only check if WiFi is reported as connected.
    if (_currentStatus != CONNECTED || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    // Set a timeout for the connection attempt (optional, default is usually reasonable)
    http.setConnectTimeout(5000); // 5 seconds
    bool success = false;

    if (http.begin(testUrl)) { // Use if to check if begin was successful
        int httpCode = http.GET(); // Perform the GET request

        // Consider HTTP_CODE_NO_CONTENT (204) used by generate_204 as success
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT) {
            success = true;
        }
        // else { Serial.printf("Internet check failed, HTTP code: %d\n", httpCode); } // Optional Debug

        http.end(); // End the connection
    }
    // else { Serial.println("Internet check: http.begin failed"); } // Optional Debug

    return success;
}

// Sets the WiFi credentials to be used for connection attempts.
void WiFiManager::setCredentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
}