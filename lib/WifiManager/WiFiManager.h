#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>  // Included for checkInternetConnection potentially
#include "LEDStatus.h"   // For status indication via LED

// --- WiFi Connection Parameters ---
#define WIFI_CONNECT_TIMEOUT        20000  ///< Max time in ms to wait for connection before considering it potentially lost in handleWiFi().
#define WIFI_RECONNECT_INTERVAL     5000   ///< Time in ms between automatic reconnection attempts.
#define MAX_WIFI_RECONNECT_ATTEMPTS 5      ///< Max number of automatic reconnection attempts before setting status to FAILED.
// ----------------------------------

/**
 * @file WiFiManager.h
 * @brief Manages WiFi station (STA) connection state and handles non-blocking reconnection.
 *
 * This class encapsulates WiFi connection logic, including:
 * - Storing credentials.
 * - Initiating connections.
 * - Tracking connection status via a state machine.
 * - Handling WiFi events (connect, disconnect, got IP) via static callbacks.
 * - Implementing non-blocking, timed reconnection attempts.
 * - Providing a method to check basic internet connectivity.
 * It uses a singleton pattern internally (`_instance`) to allow static event handlers
 * to update the state of the active WiFiManager object.
 */
class WiFiManager {
public:
    /**
     * @brief Defines the possible WiFi connection states managed by the class.
     */
    enum ConnectionStatus {
        DISCONNECTED,       ///< Not connected and no connection attempt in progress.
        CONNECTING,         ///< Connection attempt initiated and in progress.
        CONNECTED,          ///< Successfully connected to the AP and obtained an IP address.
        CONNECTION_FAILED,  ///< Reached maximum reconnection attempts without success.
        CONNECTION_LOST     ///< Was connected, but the connection was lost (disconnected event received).
    };

    /**
     * @brief Constructor for WiFiManager.
     */
    WiFiManager();

    /**
     * @brief Initializes basic WiFi configuration.
     * Sets the ESP32 to Station (STA) mode and disables the built-in auto-reconnect
     * (as this class handles reconnection logic).
     * @return True (currently always returns true).
     */
    bool begin();

    /**
     * @brief Initiates an attempt to connect to the configured WiFi network.
     * Sets the status to CONNECTING and calls WiFi.begin().
     * Does nothing if already CONNECTED or CONNECTING.
     * @return True if the connection attempt was initiated or already connected/connecting, False otherwise (though currently always returns true if not already connected/connecting).
     */
    bool connectToWiFi();

    /**
     * @brief Gets the current WiFi connection status.
     * @return The current state as defined in the ConnectionStatus enum.
     */
    ConnectionStatus getConnectionStatus();

    /**
     * @brief Forces a disconnection from the WiFi network.
     * Sets the status to DISCONNECTED.
     */
    void disconnect();

    /**
     * @brief Handles the WiFi state machine, including reconnection logic.
     * This method should be called repeatedly in the main loop.
     * It checks if a reconnection attempt is needed based on the current state
     * and the time elapsed since the last attempt or connection loss.
     */
    void handleWiFi();

    /**
     * @brief Performs a basic check for internet connectivity.
     * Makes an HTTP GET request to a known server (Google's generate_204 by default)
     * to see if a valid HTTP response is received.
     * @param testUrl The URL to use for the connectivity check.
     * @return True if connected to WiFi and the test URL returns HTTP 200 or 204, False otherwise.
     */
    bool checkInternetConnection(const String& testUrl = "http://clients3.google.com/generate_204");

    /**
     * @brief Sets the WiFi credentials to be used for connection attempts.
     * @param ssid The WiFi SSID.
     * @param password The WiFi Password.
     */
    void setCredentials(const String& ssid, const String& password);

private:
    LEDStatus _led;                     ///< Instance of LEDStatus for visual feedback.
    unsigned long _lastReconnectAttempt = 0; ///< Timestamp (millis) of the last reconnection attempt or disconnect event.
    unsigned int _reconnectAttempts = 0;  ///< Counter for automatic reconnection attempts.
    ConnectionStatus _currentStatus = DISCONNECTED; ///< Current state of the WiFi connection.
    static WiFiManager* _instance;      ///< Singleton instance pointer for static callbacks.

    // Store copies of credentials using String objects
    String _ssid;                       ///< The target WiFi network SSID.
    String _password;                   ///< The target WiFi network password.

    // --- Static Event Handlers ---
    // These functions are called by the ESP32 WiFi system events.
    // They use the static '_instance' pointer to update the state of the
    // active WiFiManager object.

    /**
     * @brief Static callback function for WiFi STA connected event.
     * (Currently does nothing, waits for IP assignment).
     * @param event The WiFi event type.
     * @param info Event-specific information.
     */
    static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Static callback function for WiFi STA got IP event.
     * Updates the status to CONNECTED and resets reconnection attempts.
     * @param event The WiFi event type.
     * @param info Event-specific information containing IP details.
     */
    static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Static callback function for WiFi STA disconnected event.
     * Updates the status to CONNECTION_LOST and records the time.
     * @param event The WiFi event type.
     * @param info Event-specific information containing the reason for disconnection.
     */
    static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    // --- End Static Event Handlers ---

    /**
     * @brief Internal method to attempt reconnection if conditions are met.
     * Called by handleWiFi(). Checks interval and retry limits.
     */
    void attemptReconnect();
};

#endif // WIFI_MANAGER_H