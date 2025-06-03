/**
 * @file EnvironmentDataJSON.cpp
 * @brief Implements the static method of the EnvironmentDataJSON utility class.
 */
#include "EnvironmentDataJSON.h"
#include <WiFi.h> // For WiFi.status() check

// HTTP Timeout for environmental data requests (milliseconds)
#define ENV_DATA_HTTP_REQUEST_TIMEOUT 10000

/**
 * @brief Static method implementation for constructing JSON and sending environmental data via HTTP POST.
 */
int EnvironmentDataJSON::IOEnvironmentData(
    const String& fullEnvDataUrl,
    const String& accessToken,
    float lightLevel,
    float temperature,
    float humidity
) {
    // Basic validation of inputs
    if (fullEnvDataUrl.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Skipped sending: Missing fullEnvDataUrl."));
        #endif
        return -1; // Indicate client-side error: missing URL
    }

    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Skipped sending: No WiFi connection."));
        #endif
        return -2; // Indicate client-side error: no WiFi
    }

    // Create a JSON document.
    JsonDocument doc;
    doc["light"] = lightLevel;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    if (jsonPayload.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Failed to serialize JSON payload."));
        #endif
        return -3; // Indicate client-side error: JSON serialization failed
    }

    HTTPClient http;
    int httpResponseCode = -4; // Default to a generic client error

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[EnvDataJSON] Attempting to send env data. URL: %s\n", fullEnvDataUrl.c_str());
        Serial.printf("[EnvDataJSON] Payload: %s\n", jsonPayload.c_str());
    #endif

    http.setReuse(false);
    if (http.begin(fullEnvDataUrl)) { // For HTTP. For HTTPS, use WiFiClientSecure.
        http.setTimeout(ENV_DATA_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Connection", "close");
        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[EnvDataJSON] Warning: Sending environmental data without an access token."));
            #endif
        }

        httpResponseCode = http.POST(jsonPayload);

        #ifdef ENABLE_DEBUG_SERIAL
            if (httpResponseCode > 0) {
                Serial.printf("[EnvDataJSON] HTTP Response Code: %d\n", httpResponseCode);
                String responseBody = http.getString(); // API for env data returns 204 No Content typically
                if (!responseBody.isEmpty()) Serial.printf("[EnvDataJSON] Response: %s\n", responseBody.c_str());
            } else {
                Serial.printf("[EnvDataJSON] HTTP POST failed, error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            }
        #endif
        // Ensure response body is consumed if not already read for debug
        if (httpResponseCode > 0 && http.getSize() > 0) {
             http.getString();
        }

        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[EnvDataJSON] HTTP connection failed for URL: %s\n", fullEnvDataUrl.c_str());
        #endif
        httpResponseCode = -5; // Indicate client-side error: HTTP begin failed
    }

    return httpResponseCode;
}