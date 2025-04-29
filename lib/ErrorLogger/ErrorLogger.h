/**
 * @file ErrorLogger.h
 * @brief Defines a utility class for sending error log messages to a remote server.
 */
#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <Arduino.h> // Required for String class

/**
 * @class ErrorLogger
 * @brief Provides a static method to format and send error details as JSON via HTTP POST.
 *
 * This utility class simplifies sending structured error messages to a backend endpoint.
 * It includes checks for network connectivity and backend status before attempting to send.
 */
class ErrorLogger {
public:
    /**
     * @brief Sends an error log message to the specified API endpoint.
     *
     * Constructs a JSON payload with the error source and message:
     * `{"source": errorSource, "message": errorMessage}`
     * and sends it via HTTP POST to the given URL.
     *
     * @param apiUrl The full URL (String) of the backend logging endpoint.
     * @param errorSource A short string identifying the origin of the error (e.g., "SENSOR_INIT", "DHT_READ", "API_FAIL").
     * @param errorMessage A detailed description of the error message (String).
     * @return `true` if the log message was successfully sent AND the server responded with an
     * HTTP 2xx status code. Returns `false` if WiFi is disconnected, if the backend status
     * check (via the global `api` object) fails, if the HTTP request fails, or if the server
     * responds with a non-2xx status code.
     * @note This function depends on a globally available `api` object of class `API` to check
     * backend status via `api.checkBackendStatus()`. Ensure this object is instantiated globally.
     * @note This function currently uses an HTTP connection (`WiFiClient`). If the `apiUrl`
     * requires HTTPS, the implementation in the .cpp file must be modified to use `WiFiClientSecure`
     * and handle certificate validation appropriately.
     */
    static bool sendLog(const String& apiUrl, const String& errorSource, const String& errorMessage);
};

#endif // ERRORLOGGER_H