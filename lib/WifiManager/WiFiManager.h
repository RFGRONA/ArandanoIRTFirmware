/**
 * @file WiFiManager.h
 * @brief Defines a class to manage WiFi station (STA) connection state and handle non-blocking reconnection.
 *
 * This class encapsulates WiFi connection logic for an ESP32 in Station mode, including:
 * - Storing network credentials (SSID, password).
 * - Initiating connection attempts.
 * - Tracking the connection status via a state machine (`ConnectionStatus` enum).
 * - Handling standard WiFi system events (connect, disconnect, got IP) via static callbacks.
 * - Implementing non-blocking, timed reconnection attempts with configurable limits.
 * - Providing a method to perform a basic check for internet connectivity.
 * It uses an internal singleton pattern (`_instance`) to allow the static event handlers
 * (required by the ESP32 WiFi event system) to correctly update the state of the
 * single active WiFiManager object. It also leverages the LEDStatus class for visual feedback.
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>          // ESP32 WiFi library
#include <HTTPClient.h>    // Required for checkInternetConnection method
#include "LEDStatus.h"     // Required for status indication via LED

// --- WiFi Connection Timing and Retry Parameters ---
#define WIFI_CONNECT_TIMEOUT        20000  ///< Max time in ms to wait in CONNECTING state before timing out the attempt.
#define WIFI_RECONNECT_INTERVAL     5000   ///< Time in ms to wait between automatic reconnection attempts after a disconnect.
#define MAX_WIFI_RECONNECT_ATTEMPTS 5      ///< Max number of automatic reconnection attempts before setting status to CONNECTION_FAILED.
// -----------------------------------------------------

/**
 * @class WiFiManager
 * @brief Manages WiFi connection lifecycle, state, and automatic reconnection.
 */
class WiFiManager {
public:
    /**
     * @brief Defines the possible states of the WiFi connection managed by this class.
     */
    enum ConnectionStatus {
        DISCONNECTED,      ///< State: Not connected and no connection attempt currently in progress.
        CONNECTING,        ///< State: A connection attempt has been initiated and is actively in progress.
        CONNECTED,         ///< State: Successfully connected to the Access Point and obtained an IP address.
        CONNECTION_FAILED, ///< State: Reached the maximum number of reconnection attempts without success. Requires manual intervention or restart.
        CONNECTION_LOST    ///< State: Was previously connected, but the connection was lost (disconnected event received). Will attempt reconnection.
    };

    /**
     * @brief Constructor for WiFiManager.
     * Initializes internal state and registers WiFi event callbacks.
     */
    WiFiManager();

    /**
     * @brief Performs basic WiFi initialization.
     * Sets the ESP32 to Station (STA) mode. Crucially, it disables the ESP-IDF's
     * built-in auto-reconnect mechanism (`WiFi.setAutoReconnect(false)`), as this
     * class implements its own custom reconnection logic in `handleWiFi()`.
     * @return `true` (currently always returns true, assuming basic setup succeeds).
     */
    bool begin();

    /**
     * @brief Initiates an attempt to connect to the configured WiFi network.
     * If the current status is `DISCONNECTED` or `CONNECTION_LOST`, this method
     * sets the status to `CONNECTING`, updates the LED, resets reconnection counters,
     * and calls `WiFi.begin()` with the stored credentials.
     * Does nothing if the status is already `CONNECTED` or `CONNECTING`.
     * @return `true` if a connection attempt was newly initiated. Returns `true` if already `CONNECTED`.
     * Returns `false` if currently in the `CONNECTING` state (no new attempt started).
     * The return value might differ slightly from this depending on implementation details. Check .cpp.
     */
    bool connectToWiFi();

    /**
     * @brief Retrieves the current WiFi connection status.
     * @return The current connection state as defined in the `ConnectionStatus` enum.
     */
    ConnectionStatus getConnectionStatus();

    /**
     * @brief Forces an immediate disconnection from the WiFi network.
     * Calls `WiFi.disconnect()` and sets the internal status to `DISCONNECTED`.
     * May also update the LED to indicate an error or disconnected state.
     */
    void disconnect();

    /**
     * @brief Handles the WiFi state machine, including reconnection logic and timeouts.
     * This method **must be called repeatedly** in the main application loop (`loop()`).
     * It checks if reconnection is needed based on the current state (`DISCONNECTED`,
     * `CONNECTION_LOST`, `CONNECTION_FAILED` but not permanently failed) and timers.
     * It also checks for connection timeouts when in the `CONNECTING` state.
     */
    void handleWiFi();

    /**
     * @brief Performs a basic check for internet connectivity by sending an HTTP GET request.
     * Sends a request to a specified URL (defaults to Google's "generate_204" endpoint,
     * which returns HTTP 204 No Content on success). This verifies not only WiFi
     * connection but also basic network routing and DNS resolution.
     * @param testUrl The URL (String) to use for the connectivity check. Defaults to "http://clients3.google.com/generate_204".
     * @return `true` only if the device is currently `CONNECTED` to WiFi AND the HTTP GET
     * request to `testUrl` returns a success code (HTTP 200 OK or 204 No Content).
     * Returns `false` in all other cases (not connected, HTTP request fails, or server returns error).
     */
    bool checkInternetConnection(const String& testUrl = "http://clients3.google.com/generate_204");

    /**
     * @brief Sets the WiFi credentials (SSID and password) to be used for connection attempts.
     * @param ssid The WiFi network name (SSID) as a String.
     * @param password The WiFi network password as a String.
     */
    void setCredentials(const String& ssid, const String& password);

private:
    LEDStatus _led;                     ///< @brief Instance of LEDStatus for visual feedback of connection state.
    unsigned long _lastReconnectAttempt = 0; ///< @brief Timestamp (from millis()) of the last reconnection attempt or disconnect event. Used for timing intervals.
    unsigned int _reconnectAttempts = 0;  ///< @brief Counter tracking the number of consecutive automatic reconnection attempts made.
    ConnectionStatus _currentStatus = DISCONNECTED; ///< @brief Holds the current state of the WiFi connection according to the `ConnectionStatus` enum.
    static WiFiManager* _instance;      ///< @brief Static pointer to the single instance of this class, used by static event handlers. (Singleton Pattern).

    // Store copies of credentials internally
    String _ssid;                       ///< @brief Stores the target WiFi network SSID.
    String _password;                   ///< @brief Stores the target WiFi network password.

    // --- Static Event Handlers ---
    // These static functions are registered with the ESP32 WiFi event system.
    // When an event occurs, the corresponding static function is called.
    // Inside, it uses the static '_instance' pointer to call methods or update
    // members of the actual WiFiManager object.

    /**
     * @brief Static callback function registered for the `ARDUINO_EVENT_WIFI_STA_CONNECTED` event.
     * Indicates successful association with the Access Point, but usually before an IP address is assigned.
     * (Currently, this handler does minimal work, waiting for the `GOT_IP` event).
     * @param event The WiFi event type (`ARDUINO_EVENT_WIFI_STA_CONNECTED`).
     * @param info Event-specific information structure (`WiFiEventInfo_t`).
     */
    static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Static callback function registered for the `ARDUINO_EVENT_WIFI_STA_GOT_IP` event.
     * This is the primary indicator of a successful connection. Updates the status to `CONNECTED`,
     * resets reconnection attempt counters, and updates the status LED via the `_instance`.
     * @param event The WiFi event type (`ARDUINO_EVENT_WIFI_STA_GOT_IP`).
     * @param info Event-specific information structure (`WiFiEventInfo_t`) containing IP details.
     */
    static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Static callback function registered for the `ARDUINO_EVENT_WIFI_STA_DISCONNECTED` event.
     * Indicates loss of connection to the Access Point. Updates the status to `CONNECTION_LOST`
     * (if previously connected), records the time for reconnection logic, and updates the LED
     * via the `_instance`.
     * @param event The WiFi event type (`ARDUINO_EVENT_WIFI_STA_DISCONNECTED`).
     * @param info Event-specific information structure (`WiFiEventInfo_t`) containing the reason code for disconnection.
     */
    static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    // --- End Static Event Handlers ---

    /**
     * @brief Internal helper method to perform a reconnection attempt.
     * Called by `handleWiFi()` when conditions indicate a retry is needed (correct state,
     * interval elapsed, max retries not exceeded). Sets state to `CONNECTING` and calls `WiFi.begin()`.
     * Updates state to `CONNECTION_FAILED` if max retries are hit.
     */
    void attemptReconnect();
};

#endif // WIFI_MANAGER_H