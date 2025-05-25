/**
 * @file ErrorLogger.h
 * @brief Defines a utility class for sending structured log messages to a remote server.
 *
 * Provides a static method to format log messages (INFO, WARNING, ERROR) as JSON
 * and send them via an authenticated HTTP POST request.
 */
#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <Arduino.h> // Required for String class

// Define standard log types as const char* for efficiency and consistency
const char LOG_TYPE_INFO[] = "INFO";
const char LOG_TYPE_WARNING[] = "WARNING";
const char LOG_TYPE_ERROR[] = "ERROR";

/**
 * @class ErrorLogger
 * @brief Utility class with a static method for sending log messages.
 *
 * This class handles formatting log data into JSON and POSTing it to a specified endpoint.
 * It requires the caller to provide the full URL, an access token for authorization,
 * the log type, and the message.
 */
class ErrorLogger {
public:
    /**
     * @brief Sends a log message to the specified API endpoint.
     *
     * Constructs a JSON payload with the structure:
     * `{"logType": "TYPE_HERE", "logMessage": "Your message here"}`
     * and sends it via HTTP POST to the given URL using an access token for authorization.
     * The caller is responsible for ensuring WiFi is connected and the accessToken is valid
     * before calling this method.
     *
     * @param fullLogUrl The complete URL (String) of the backend logging endpoint.
     * @param accessToken The access token (String) for `Authorization: Device <token>` header.
     * If empty or null, the request might be sent without authorization,
     * depending on server configuration (typically logs require auth).
     * @param logType The type of log (e.g., "INFO", "WARNING", "ERROR"). Use the predefined
     * LOG_TYPE_INFO, LOG_TYPE_WARNING, LOG_TYPE_ERROR constants.
     * @param logMessage The detailed log message (String).
     * @return `true` if the log message was successfully sent AND the server responded with an
     * HTTP 2xx status code. Returns `false` if the HTTP request fails, or if the server
     * responds with a non-2xx status code, or if essential parameters are missing.
     */
    static bool sendLog(const String& fullLogUrl, const String& accessToken, const char* logType, const String& logMessage);
};

#endif // ERRORLOGGER_H