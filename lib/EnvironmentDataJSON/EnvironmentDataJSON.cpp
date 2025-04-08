#include "EnvironmentDataJSON.h"

/**
 * @brief Static method implementation for sending environmental data.
 */
bool EnvironmentDataJSON::IOEnvironmentData(
    const String& apiUrl,
    float lightLevel,
    float temperature,
    float humidity
) {
    // Create a JSON document to hold the sensor data.
    // Using JsonDocument as StaticJsonDocument is deprecated.
    // Size estimate should be sufficient for the three key-value pairs.
    JsonDocument doc; // Adjusted from StaticJsonDocument

    // Populate the JSON document
    doc["light"] = lightLevel;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    // Serialize the JSON document into a String
    String jsonString;
    serializeJson(doc, jsonString);

    // Prepare the HTTP client
    HTTPClient http;
    bool success = false; // Flag to track overall success

    // Initialize the HTTP connection
    if (http.begin(apiUrl)) { // Use if statement for better error handling if begin fails

        http.addHeader("Content-Type", "application/json"); // Set the content type header

        // Send the HTTP POST request with the JSON payload
        int httpResponseCode = http.POST(jsonString);

        // Check the HTTP response code
        if (httpResponseCode > 0) {
            // Read the response body (even if not used) to clear the buffer
            String payload = http.getString();
            // Serial.printf("[HTTP] POST Response Code: %d\n", httpResponseCode); // Optional debug output
            // Serial.println("[HTTP] POST Response Body: " + payload); // Optional debug output

            // Check if the response code indicates success (2xx range)
            if (httpResponseCode >= 200 && httpResponseCode < 300) {
                success = true;
            } else {
                // Handle non-2xx responses if needed
                // Serial.printf("[HTTP] POST failed, server response code: %d\n", httpResponseCode);
                success = false;
            }
        } else {
            // Handle connection or request sending errors
            // Serial.printf("[HTTP] POST failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
            success = false;
        }

        // End the HTTP connection
        http.end();

    } else {
        // Handle failure to begin HTTP connection
        // Serial.printf("[HTTP] Unable to connect to %s\n", apiUrl.c_str());
        success = false;
    }

    return success; // Return the final success status
}