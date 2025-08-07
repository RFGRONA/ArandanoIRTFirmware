// lib/ErrorLogger/ErrorLogger.h
#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <Arduino.h>

// Forward declaration to ensure SDManager and its nested types are recognized
class SDManager;
class TimeManager;
enum class LogLevel;

// Define standard log types as const char* for efficiency and consistency
// These should already be here from your existing ErrorLogger.h
extern const char LOG_TYPE_INFO[];
extern const char LOG_TYPE_WARNING[];
extern const char LOG_TYPE_ERROR[];

class ErrorLogger {
public:
    /**
     * @brief Sends a log message locally to SD card and optionally to a remote API endpoint.
     *
     * Constructs a JSON payload for remote logging. Always attempts to save the log
     * to the SD card first. If WiFi is connected and accessToken is provided,
     * it then attempts to send the log to the specified API URL.
     *
     * @param sdManager Reference to the SDManager instance for local logging.
     * @param timeManager Reference to the TimeManager instance for timestamps.
     * @param fullLogUrl The complete URL (String) of the backend logging endpoint.
     * @param accessToken The access token (String) for `Authorization: Device <token>` header for remote logging.
     * If empty, remote logging might be attempted without authorization or skipped based on API behavior.
     * @param logType The type of log (e.g., LOG_TYPE_INFO).
     * @param logMessage The detailed log message (String).
     * @param internalTemp Optional. The internal temperature reading (float).
     * @param internalHum Optional. The internal humidity reading (float).
     * @return `true` if the log was successfully saved to SD. The success of remote sending is handled internally.
     * Returns `false` if saving to SD fails or if essential parameters are missing for local logging.
     */
    static bool sendLog(SDManager& sdManager,        
                       TimeManager& timeManager,      
                       const String& fullLogUrl, 
                       const String& accessToken, 
                       const char* logType, 
                       const String& logMessage, 
                       float internalTemp = NAN, 
                       float internalHum = NAN);

/**
     * @brief Sends a log message exclusively to the local SD card.
     * This method does not attempt to send the log to any remote API.
     * It's intended for critical local diagnostics or events where network
     * connectivity is not available, not desired, or has failed.
     *
     * @param sdManager Reference to the SDManager instance for local logging.
     * @param timeManager Reference to the TimeManager instance for timestamps.
     * @param level The severity level of the log message (INFO, WARNING, ERROR from LogLevel enum in SDManager.h).
     * @param logMessage The detailed log message (String).
     * @param internalTemp Optional. The internal temperature reading (float).
     * @param internalHum Optional. The internal humidity reading (float).
     * @return `true` if the log was successfully saved to SD. 
     * Returns `false` if saving to SD fails or if essential parameters are missing.
     */
    static bool logToSdOnly(SDManager& sdManager,
                            TimeManager& timeManager,
                            LogLevel level,
                            const String& logMessage,
                            float internalTemp = NAN,
                            float internalHum = NAN);
};

#endif // ERRORLOGGER_H