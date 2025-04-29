/**
 * @file ErrorLogger.cpp
 * @brief Implements the ErrorLogger utility class for sending remote error logs.
 * @note This implementation relies on the ESP32 WiFi and HTTPClient libraries, ArduinoJson,
 * and requires WiFi to be connected for logs to be sent.
 * @note It also depends on a globally instantiated `api` object (from API.h/API.cpp)
 * to check backend status via `api.checkBackendStatus()` before attempting to send logs.
 */
#include "ErrorLogger.h"
#include <ArduinoJson.h>    // For JSON formatting
#include <HTTPClient.h>     // For making HTTP requests
#include <WiFi.h>           // For checking WiFi status (WiFi.status())
#include "API.h"            // Required for the global 'api' object instance

// Global instance dependency: Assumes an 'api' object of class API is defined globally.
extern API api; // Declaration that 'api' is defined elsewhere (usually in the main .ino or another .cpp)
                // If 'API api;' is defined here instead, it creates a *new* instance just for this file,
                // which might not be intended if 'api' should be shared across the project.
                // Using 'extern' assumes it's defined once globally somewhere else.
                // **If 'API api;' was intended to be defined here globally, keep it, but be aware of potential issues if API state needs to be shared.**

/**
 * @brief Static method implementation for sending structured error logs.
 */
bool ErrorLogger::sendLog(const String& apiUrl, const String& errorSource, const String& errorMessage) {

    // Do NOT attempt to send logs if WiFi is not connected or if the backend check fails.
    if (WiFi.status() != WL_CONNECTED || !api.checkBackendStatus()) {
        // Optional: Log locally (Serial) that the remote log was skipped.
        // Serial.println("[ErrorLogger] Skipped sending log: No WiFi or backend unavailable.");
        return false;
    }

    // Create JSON document dynamically.
    JsonDocument doc;
    // Populate the JSON object with error source and message.
    doc["source"] = errorSource;
    doc["message"] = errorMessage;
    // Note: Timestamp (e.g., using millis() or an RTC) is not included here,
    // but could be added if needed: doc["timestamp"] = millis();

    // Serialize JSON document to a String.
    String jsonString;
    serializeJson(doc, jsonString);

    // Configure and send the HTTP POST request.
    HTTPClient http;
    bool success = false;

    // Use a standard WiFiClient for HTTP connections.
    // If apiUrl requires HTTPS, WiFiClientSecure must be used here, along with
    // proper certificate handling (e.g., client.setCACert, client.setInsecure - not recommended).
    WiFiClient client;
    // Use the 'begin' version that takes a client instance, suitable for both HTTP/HTTPS.
    if (http.begin(client, apiUrl)) { // Check if begin was successful

        // Set the content type header to indicate JSON payload.
        http.addHeader("Content-Type", "application/json");

        // Perform the HTTP POST request.
        int httpResponseCode = http.POST(jsonString);

        // Check if the server responded with a success code (2xx range).
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            success = true;
            // Optional: Log successful send: Serial.println("[ErrorLogger] Log sent successfully.");
        } else {
            success = false;
            // Optional: Log failure details:
            // Serial.printf("[ErrorLogger] Failed to send log. HTTP Response Code: %d\n", httpResponseCode);
            // if(httpResponseCode > 0) { Serial.println("[ErrorLogger] Response Body: " + http.getString()); }
            // else { Serial.printf("[ErrorLogger] HTTP Error: %s\n", http.errorToString(httpResponseCode).c_str()); }
        }

        // Release HTTP resources.
        http.end();

    } else {
        // Failed to initialize the HTTP connection.
        // Serial.printf("[ErrorLogger] HTTP connection failed for URL: %s\n", apiUrl.c_str());
        success = false;
    }

    return success;
}