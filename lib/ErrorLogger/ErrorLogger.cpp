// lib/ErrorLogger/ErrorLogger.cpp
#include "ErrorLogger.h"
#include <ArduinoJson.h> 
#include <HTTPClient.h>  
#include <WiFi.h>        
#include <math.h>        
#include "SDManager.h"    
#include "TimeManager.h"  

#define LOG_HTTP_REQUEST_TIMEOUT 5000 

// Define the log type constants here (this is the single point of definition)
const char LOG_TYPE_INFO[]    = "INFO";
const char LOG_TYPE_WARNING[] = "WARNING";
const char LOG_TYPE_ERROR[]   = "ERROR";

bool ErrorLogger::sendLog(SDManager& sdManager,
                          TimeManager& timeManager,
                          const String& fullLogUrl, 
                          const String& accessToken, 
                          const char* logType, 
                          const String& logMessage, 
                          float internalTemp, 
                          float internalHum) {

    // --- Step 1: Basic Parameter Validation ---
    if (logType == nullptr || logMessage.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Skipped logging: Missing logType or message."));
        #endif
        return false; // Cannot log without type or message
    }

    // --- Step 2: Get Timestamp ---
    String timestamp = timeManager.getCurrentTimestampString(); // Format: "YYYY-MM-DD HH:MM:SS" or uptime based if not synced

    // --- Step 3: Log Locally to SD Card (Always Attempt) ---
    bool localLogSuccess = false;
    if (sdManager.isSDAvailable()) { //
        // Convert const char* logType to LogLevel enum for sdManager.logToFile
        LogLevel levelEnum;
        if (strcmp(logType, LOG_TYPE_INFO) == 0) {
            levelEnum = LogLevel::INFO;
        } else if (strcmp(logType, LOG_TYPE_WARNING) == 0) {
            levelEnum = LogLevel::WARNING;
        } else { // Default to ERROR for unknown or LOG_TYPE_ERROR
            levelEnum = LogLevel::ERROR;
        }
        
        localLogSuccess = sdManager.logToFile(timestamp, 
                                              levelEnum,
                                              logMessage, 
                                              internalTemp, 
                                              internalHum); //
        #ifdef ENABLE_DEBUG_SERIAL
            if (localLogSuccess) {
                Serial.printf("[ErrorLogger] Log successfully written to SD card. Timestamp: %s\n", timestamp.c_str());
            } else {
                Serial.println(F("[ErrorLogger] Failed to write log to SD card."));
            }
        #endif
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] SD card not available. Cannot write log locally."));
        #endif
    }

    // --- Step 4: Attempt to Send Log to Remote API (Conditionally) ---
    if (WiFi.status() == WL_CONNECTED && !fullLogUrl.isEmpty()) { //
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] WiFi connected. Attempting to send log to remote API."));
            if (accessToken.isEmpty()) {
                Serial.println(F("[ErrorLogger] Warning: Sending remote log without an access token."));
            }
        #endif

        JsonDocument doc; 
        doc["logType"] = logType; 
        doc["logMessage"] = logMessage;
        if (!isnan(internalTemp)) { 
            doc["internalDeviceTemperature"] = serialized(String(internalTemp, 2)); 
        }
        if (!isnan(internalHum)) { 
            doc["internalDeviceHumidity"] = serialized(String(internalHum, 1));
        }

        String jsonPayload;
        serializeJson(doc, jsonPayload);

        if (jsonPayload.isEmpty()) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[ErrorLogger] Failed to serialize JSON payload for remote log. Remote send skipped."));
            #endif
            return localLogSuccess; // Return status of local logging
        }

        HTTPClient http;
        http.setReuse(false);

        if (http.begin(fullLogUrl)) {
            http.setTimeout(LOG_HTTP_REQUEST_TIMEOUT);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Connection", "close");
            if (!accessToken.isEmpty()) {
                http.addHeader("Authorization", "Device " + accessToken);
            }

            int httpResponseCode = http.POST(jsonPayload);

            if (httpResponseCode >= 200 && httpResponseCode < 300) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[ErrorLogger] Remote log sent successfully. HTTP Response: %d\n", httpResponseCode);
                #endif
                // remoteLogSuccess = true;
            } else {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[ErrorLogger] Failed to send remote log. HTTP Code: %d\n", httpResponseCode);
                    if(httpResponseCode > 0) {
                        String errorResponse = http.getString();
                        Serial.printf("[ErrorLogger] Server Error: %s\n", errorResponse.c_str());
                    } else {
                        Serial.printf("[ErrorLogger] HTTP Client Error: %s\n", http.errorToString(httpResponseCode).c_str());
                    }
                #endif
            }
            http.end();
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[ErrorLogger] HTTP connection failed for remote log URL: %s\n", fullLogUrl.c_str());
            #endif
        }
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("[ErrorLogger] WiFi not connected. Remote log not sent."));
            }
            if (fullLogUrl.isEmpty()) {
                 Serial.println(F("[ErrorLogger] Remote log URL is empty. Remote log not sent."));
            }
        #endif
    }
    
    return localLogSuccess; // Primarily report success of critical local logging
}

bool ErrorLogger::logToSdOnly(SDManager& sdManager,
                              TimeManager& timeManager,
                              LogLevel level,
                              const String& logMessage,
                              float internalTemp,
                              float internalHum) {
    // --- Step 1: Basic Parameter Validation ---
    if (logMessage.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLoggerSdOnly] Skipped logging: Missing message."));
        #endif
        return false;
    }

    if (!sdManager.isSDAvailable()) { //
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLoggerSdOnly] SD card not available. Cannot write log."));
        #endif
        return false;
    }

    // --- Step 2: Get Timestamp ---
    String timestamp = timeManager.getCurrentTimestampString(); //

    // --- Step 3: Log Locally to SD Card ---
    bool localLogSuccess = sdManager.logToFile(timestamp, 
                                               level,
                                               logMessage, 
                                               internalTemp, 
                                               internalHum); //
    #ifdef ENABLE_DEBUG_SERIAL
        if (localLogSuccess) {
            Serial.printf("[ErrorLoggerSdOnly] Log successfully written to SD card. Timestamp: %s\n", timestamp.c_str());
        } else {
            Serial.println(F("[ErrorLoggerSdOnly] Failed to write log to SD card."));
        }
    #endif

    return localLogSuccess;
}