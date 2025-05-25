/**
 * @file ErrorLogger.cpp
 * @brief Implements the ErrorLogger utility class for sending remote logs.
 */
#include "ErrorLogger.h"
#include <ArduinoJson.h> // For JSON formatting
#include <HTTPClient.h>  // For making HTTP requests
#include <WiFi.h>        // For checking WiFi status (though decision to send is external)

// HTTP Timeout for log requests (milliseconds)
#define LOG_HTTP_REQUEST_TIMEOUT 5000 // Shorter timeout for logs might be acceptable

/**
 * @brief Static method implementation for sending structured log messages.
 */
bool ErrorLogger::sendLog(const String& fullLogUrl, const String& accessToken, const char* logType, const String& logMessage) {

    // Basic validation of inputs
    if (fullLogUrl.isEmpty() || logType == nullptr || logMessage.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Skipped sending log: Missing URL, logType, or message."));
        #endif
        return false;
    }

    // Although the decision to call sendLog should consider WiFi, a check here is a safeguard.
    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Skipped sending log: No WiFi connection."));
        #endif
        return false;
    }

    // If no access token is provided, logs might fail if endpoint requires auth.
    // For now, we proceed but log a warning if debug is enabled.
    if (accessToken.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Warning: Sending log without an access token."));
        #endif
    }

    // Create JSON document. DynamicJsonDocument is safer for varying message lengths.
    // Adjust capacity as needed, but this should be enough for typical log messages.
    JsonDocument doc; // Using dynamic allocation

    // Populate the JSON object.
    doc["logType"] = logType;
    doc["logMessage"] = logMessage;
    // logTimestamp is set by the backend as per specifications.

    // Serialize JSON document to a String.
    String jsonPayload;
    serializeJson(doc, jsonPayload);

    if (jsonPayload.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Failed to serialize JSON payload for log."));
        #endif
        return false;
    }

    HTTPClient http;
    bool success = false;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[ErrorLogger] Attempting to send log. Type: %s, URL: %s\n", logType, fullLogUrl.c_str());
        Serial.printf("[ErrorLogger] Payload: %s\n", jsonPayload.c_str()); // Potentially long
    #endif

    if (http.begin(fullLogUrl)) { // For HTTP. For HTTPS, use WiFiClientSecure.
        http.setTimeout(LOG_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        }

        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[ErrorLogger] Log sent successfully. HTTP Response: %d\n", httpResponseCode);
                String responseBody = http.getString(); // Usually 204 No Content, so body might be empty.
                if (!responseBody.isEmpty()) Serial.printf("[ErrorLogger] Response: %s\n", responseBody.c_str());
            #endif
            success = true;
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[ErrorLogger] Failed to send log. HTTP Response Code: %d\n", httpResponseCode);
                if(httpResponseCode > 0) {
                    String errorResponse = http.getString();
                    Serial.printf("[ErrorLogger] Server Error Response: %s\n", errorResponse.c_str());
                } else {
                    Serial.printf("[ErrorLogger] HTTP Client Error: %s\n", http.errorToString(httpResponseCode).c_str());
                }
            #endif
            success = false;
        }
        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ErrorLogger] HTTP connection failed for log URL: %s\n", fullLogUrl.c_str());
        #endif
        success = false;
    }

    return success;
}