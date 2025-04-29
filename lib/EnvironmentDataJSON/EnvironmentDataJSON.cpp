/**
 * @file EnvironmentDataJSON.cpp
 * @brief Implements the static method of the EnvironmentDataJSON utility class.
 */
#include "EnvironmentDataJSON.h"

/**
 * @brief Static method implementation for constructing JSON and sending environmental data via HTTP POST.
 */
bool EnvironmentDataJSON::IOEnvironmentData(
    const String& apiUrl,
    float lightLevel,
    float temperature,
    float humidity
) {
    // Create a JSON document to hold the sensor data.
    // Using JsonDocument for dynamic memory allocation (recommended for ESP32 with ArduinoJson v6+).
    // Adjust size calculation if more fields are added later. Default is usually sufficient for small objects.
    JsonDocument doc;

    // Populate the JSON document with sensor readings.
    // Keys are "light", "temperature", and "humidity".
    doc["light"] = lightLevel;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    // Serialize the JSON document into a String format.
    String jsonString;
    serializeJson(doc, jsonString);

    // Prepare the HTTP client for making the request.
    HTTPClient http;
    bool success = false; // Initialize success flag to false.

    // Initialize the HTTP connection to the target API URL.
    // Use an if statement to handle potential errors during initialization (e.g., invalid URL).
    // For HTTPS, ensure the ESP32 has the necessary root CA certificate or use insecure mode (not recommended).
    if (http.begin(apiUrl)) {

        // Set the HTTP header to indicate that the request body is JSON.
        http.addHeader("Content-Type", "application/json");

        // Send the HTTP POST request with the serialized JSON string as the payload.
        int httpResponseCode = http.POST(jsonString);

        // Check the HTTP response code returned by the server.
        if (httpResponseCode > 0) { // Check if response code is valid (positive value)
            // Optional: Read the response body. Useful for debugging or if the API returns data.
            String payload = http.getString();
            // Serial.printf("[HTTP] POST Response Code: %d\n", httpResponseCode);
            // Serial.println("[HTTP] POST Response Body: " + payload);

            // Check if the response code indicates success (typically 2xx range).
            if (httpResponseCode >= 200 && httpResponseCode < 300) {
                success = true; // Mark as successful if status code is 2xx.
            } else {
                // The server responded, but with an error status code (e.g., 4xx, 5xx).
                // Serial.printf("[HTTP] POST failed, server response code: %d\n", httpResponseCode);
                success = false;
            }
        } else {
            // An error occurred during the HTTP request (e.g., connection failed, request timed out).
            // httpResponseCode will be negative. http.errorToString() can provide details.
            // Serial.printf("[HTTP] POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
            success = false;
        }

        // End the HTTP connection and release resources.
        http.end();

    } else {
        // Failed to initialize the HTTP connection (e.g., invalid URL format, DNS resolution failure).
        // Serial.printf("[HTTP] Unable to begin connection to %s\n", apiUrl.c_str());
        success = false;
    }

    // Return the final success status based on the HTTP request outcome.
    return success;
}