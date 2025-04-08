#include "MultipartDataSender.h"
#include <ArduinoJson.h>
#include <vector> // Using std::vector to dynamically build the payload

/**
 * @brief Static method implementation for sending thermal and image data.
 */
bool MultipartDataSender::IOThermalAndImageData(
    const String& apiUrl,
    float* thermalData,
    uint8_t* jpegImage,
    size_t jpegLength
) {
    // --- Input Validation ---
    if (!thermalData || !jpegImage || jpegLength == 0) {
        // Serial.println("Error: Invalid input data for sending."); // Optional debug
        return false; // Cannot proceed with invalid data
    }

    // --- JSON Payload Creation (Thermal Data) ---
    // Use JsonDocument (preferred over deprecated DynamicJsonDocument)
    // Estimate capacity needed for 768 floats + JSON overhead. 4096 might be conservative.
    JsonDocument doc; // Size will adjust, uses heap allocation.

    // Create the nested array for temperatures using the recommended syntax
    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        // Serial.println("Error: Failed to create JSON array."); // Optional debug
        return false; // Failed to allocate or create array
    }

    // Copy thermal data into the JSON array
    const int totalPixels = 32 * 24;
    for(int i = 0; i < totalPixels; i++) {
        tempArray.add(thermalData[i]);
    }

    // Serialize the JSON document to a String
    String thermalJsonString;
    serializeJson(doc, thermalJsonString);
    // Clear the JsonDocument memory if it's large and won't be needed again soon
    doc.clear();

    // --- Multipart Payload Construction ---
    // Generate a unique boundary string for this request
    String boundary = "----WebKitFormBoundary" + String(esp_random());

    // Use a std::vector<uint8_t> to build the binary payload efficiently
    std::vector<uint8_t> payload;
    // Reserve memory to minimize reallocations (estimate size)
    payload.reserve(thermalJsonString.length() + jpegLength + 512); // Adjusted reservation slightly

    // Helper lambda function to append String data to the byte vector
    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };

    // -- Part 1: Thermal Data (JSON) --
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"thermal\"\r\n"); // Field name "thermal"
    appendString("Content-Type: application/json\r\n\r\n");          // Content type for JSON
    appendString(thermalJsonString);                                 // The actual JSON string
    appendString("\r\n");                                            // End of part

    // -- Part 2: Image Data (JPEG) --
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n"); // Field name "image", filename "camera.jpg"
    appendString("Content-Type: image/jpeg\r\n\r\n");                // Content type for JPEG
    // Append the raw JPEG byte data directly
    payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
    appendString("\r\n");                                            // End of part

    // -- Closing Boundary --
    appendString("--" + boundary + "--\r\n");                        // Final boundary marker

    // --- HTTP Client Setup and Request ---
    HTTPClient http;
    bool success = false; // Track overall success

    // Begin HTTP connection
    if (http.begin(apiUrl)) {

        // Set necessary HTTP headers for multipart/form-data
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        // Content-Length is crucial for the server to know the payload size
        http.addHeader("Content-Length", String(payload.size()));

        // Send the POST request with the constructed payload
        int httpResponseCode = -1;
        int maxRetries = 3; // Number of retries for the POST request
        for (int retry = 0; retry < maxRetries; retry++) {
            // Send the payload from the vector's data pointer and size
            httpResponseCode = http.POST(payload.data(), payload.size());

            if (httpResponseCode > 0) { // Check if connection and request were successful at HTTP level
                 // Read the response body (Task 5 fix)
                String httpPayload = http.getString();
                // Serial.printf("[HTTP] POST Response Code: %d\n", httpResponseCode); // Optional debug output
                // Serial.println("[HTTP] POST Response Body: " + httpPayload); // Optional debug output

                // Break the retry loop if we got a valid response code (even if it's an error like 4xx or 5xx)
                // We primarily retry on connection errors (httpResponseCode < 0)
                break;
            } else {
                // Serial.printf("[HTTP] POST failed on attempt %d, error: %s\n", retry + 1, http.errorToString(httpResponseCode).c_str()); // Optional debug
                delay(500); // Wait before retrying
            }
        }

        // Check if the final response code indicates success (2xx range)
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            success = true;
        }

        // End the HTTP connection
        http.end();

    } else {
        // Serial.printf("[HTTP] Unable to connect to %s\n", apiUrl.c_str()); // Optional debug
        success = false;
    }

    // The std::vector 'payload' goes out of scope here and its memory is automatically freed.

    return success; // Return the final success status
}