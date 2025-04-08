#include "MultipartDataSender.h"
#include <ArduinoJson.h>
#include <vector> // Using std::vector to dynamically build the payload

/**
 * @brief Sends thermal data and a JPEG image via HTTP POST using multipart/form-data encoding.
 * Orchestrates helper functions for JSON creation, payload building, and HTTP POSTing.
 */
bool MultipartDataSender::IOThermalAndImageData(
    const String& apiUrl,
    float* thermalData,
    uint8_t* jpegImage,
    size_t jpegLength
) {
    // 1. Input Validation
    if (!thermalData || !jpegImage || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("MultipartSender Error: Invalid input data.");
        #endif
        return false;
    }

    // 2. Create Thermal JSON Payload
    String thermalJsonString = createThermalJson(thermalData);
    if (thermalJsonString.length() == 0) {
        // Error logged within helper function
        return false;
    }

    // 3. Generate Boundary
    String boundary = "----WebKitFormBoundary" + String(esp_random());

    // 4. Build Multipart Payload
    std::vector<uint8_t> payload = buildMultipartPayload(boundary, thermalJsonString, jpegImage, jpegLength);
    if (payload.empty()) {
        // Error logged within helper function (e.g., if vector couldn't reserve memory)
        return false;
    }

    // 5. Perform HTTP POST Request
    return performHttpPost(apiUrl, boundary, payload);
}

// --- MultipartDataSender Helper Function Implementations ---

/**
 * @brief Creates a JSON string from the thermal data array.
 */
/* static */ String MultipartDataSender::createThermalJson(float* thermalData) {
    // Use JsonDocument (adjust capacity if needed, maybe calculate more precisely)
    JsonDocument doc;
    const size_t capacity = JSON_ARRAY_SIZE(32 * 24) + JSON_OBJECT_SIZE(1) + 20; // Estimate
    // If using heap: JsonDocument doc(capacity);

    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("MultipartSender Error: Failed to create JSON array for thermal data.");
        #endif
        return ""; // Return empty string on failure
    }

    const int totalPixels = 32 * 24;
    for(int i = 0; i < totalPixels; i++) {
        tempArray.add(thermalData[i]);
    }

    String jsonString;
    serializeJson(doc, jsonString);
    return jsonString;
}

/**
 * @brief Builds the complete multipart/form-data payload as a byte vector.
 */
/* static */ std::vector<uint8_t> MultipartDataSender::buildMultipartPayload(
    const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength
) {
    std::vector<uint8_t> payload;
    // Reserve memory (consider a slightly safer estimation)
    // Using thermalJson.length() which includes quotes, commas, etc.
    size_t estimatedSize = thermalJson.length() + jpegLength + 512; // Headers, boundaries etc.
    payload.reserve(estimatedSize);

    // Helper lambda to append String data
    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };

    // --- Part 1: Thermal Data (JSON) ---
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"thermal\"\r\n");
    appendString("Content-Type: application/json\r\n\r\n");
    appendString(thermalJson);
    appendString("\r\n");

    // --- Part 2: Image Data (JPEG) ---
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
    appendString("Content-Type: image/jpeg\r\n\r\n");
    // Append binary JPEG data
    payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
    appendString("\r\n");

    // --- Closing Boundary ---
    appendString("--" + boundary + "--\r\n");

    return payload;
}

/**
 * @brief Performs the actual HTTP POST request with retries.
 */
/* static */ bool MultipartDataSender::performHttpPost(
    const String& apiUrl, const String& boundary, const std::vector<uint8_t>& payload
) {
    HTTPClient http;
    bool success = false;

    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Initiating HTTP POST request to " + apiUrl + "...");
      Serial.printf("  Payload Size: %d bytes\n", payload.size());
    #endif

    if (http.begin(apiUrl)) {
        // Set Headers
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        http.addHeader("Content-Length", String(payload.size()));
        // Add any other required headers (like Authorization if needed)
        // http.addHeader("Authorization", "Bearer YOUR_TOKEN");

        int httpResponseCode = -1;
        const int maxRetries = 3;
        for (int retry = 0; retry < maxRetries; retry++) {
            #ifdef ENABLE_DEBUG_SERIAL
              if (retry > 0) Serial.printf("  Retrying POST... (Attempt %d/%d)\n", retry + 1, maxRetries);
            #endif
            httpResponseCode = http.POST(const_cast<uint8_t*>(payload.data()), payload.size());

            if (httpResponseCode > 0) { // Got a response from server (even if error)
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP Response Code: %d\n", httpResponseCode);
                #endif
                // Read response body to clear buffer
                String responseBody = http.getString();
                #ifdef ENABLE_DEBUG_SERIAL
                  // Only print body if short or if needed for debugging specific errors
                  if (responseBody.length() < 200) {
                     Serial.println("  HTTP Response Body: " + responseBody);
                  } else {
                     Serial.println("  HTTP Response Body: (Truncated - " + String(responseBody.length()) + " bytes)");
                  }
                #endif
                break; // Exit retry loop once a response is received
            } else {
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP POST failed (Attempt %d/%d), error: %s\n", retry + 1, maxRetries, http.errorToString(httpResponseCode).c_str());
                #endif
                delay(500); // Wait before retrying connection/send error
            }
        } // End retry loop

        // Check if the final response code indicates success
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            success = true;
        }

        http.end(); // End connection

    } else {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("MultipartSender Error: Unable to begin HTTP connection to " + apiUrl);
        #endif
        success = false;
    }

    return success;
}