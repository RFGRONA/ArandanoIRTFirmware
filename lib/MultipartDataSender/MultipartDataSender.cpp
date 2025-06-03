/**
 * @file MultipartDataSender.cpp
 * @brief Implements the MultipartDataSender utility class methods.
 */
#include "MultipartDataSender.h"
#include <ArduinoJson.h> // For creating the JSON part
#include <vector>        // Using std::vector to dynamically build the byte payload
#include <esp_random.h>  // For generating a random boundary component
#include <math.h>        // For INFINITY, NAN, isnan
#include <WiFi.h>        // For WiFi.status() check

#define CAPTURE_DATA_HTTP_REQUEST_TIMEOUT 20000

// --- Public Static Methods ---

/* static */ int MultipartDataSender::IOThermalAndImageData(
    const String& fullCaptureDataUrl,
    const String& accessToken,
    float* thermalData,
    uint8_t* jpegImage,
    size_t jpegLength
) {
    // --- Step 1: Validate Input Data ---
    if (fullCaptureDataUrl.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Invalid input: Missing fullCaptureDataUrl."));
        #endif
        return -11; // Custom client error
    }
    if (thermalData == nullptr || jpegImage == nullptr || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Invalid input: Null pointer or zero length image/thermal data."));
        #endif
        return -12; // Custom client error
    }
     if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MultipartSender Error] Skipped sending: No WiFi connection."));
        #endif
        return -13; // Custom client error: no WiFi
    }


    // --- Step 2: Create JSON Payload for Thermal Data ---
    String thermalJsonString = createThermalJson(thermalData);
    if (thermalJsonString.isEmpty()) { // Changed from length() == 0 for clarity
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Failed to create thermal JSON."));
        #endif
        return -14; // Custom client error: JSON creation failed
    }

    // --- Step 3: Generate a Unique Boundary String ---
    String boundary = "----WebKitFormBoundaryESP32-" + String(esp_random(), HEX) + String(esp_random(), HEX);


    // --- Step 4: Build the Complete Multipart Payload ---
    std::vector<uint8_t> payload = buildMultipartPayload(boundary, thermalJsonString, jpegImage, jpegLength);
    if (payload.empty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Failed to build multipart payload."));
        #endif
        return -15; // Custom client error: Payload build failed
    }

    // --- Step 5: Perform the HTTP POST Request ---
    return performHttpPost(fullCaptureDataUrl, accessToken, boundary, payload);
}

// --- Private Static Helper Methods ---

/**
 * @brief Creates a JSON object string including raw temperatures, max, min, and average.
 */
/* static */ String MultipartDataSender::createThermalJson(float* thermalData) {
    // Calculate statistics first using helper methods
    float maxTemp = calculateMaxTemperature(thermalData);
    float minTemp = calculateMinTemperature(thermalData);
    float avgTemp = calculateAverageTemperature(thermalData);

    // Check for calculation errors (e.g., all NaN input)
    if (isinf(maxTemp) || isinf(minTemp) || isnan(avgTemp)) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Warning] Thermal stats calculation resulted in Inf/NaN, likely due to invalid input data.");
         #endif
         // Decide how to handle this - sending default values or empty string?
         // Returning empty string to indicate failure upstream.
         return "";
    }

    // Estimate JSON capacity
    const size_t capacity = JSON_OBJECT_SIZE(4) + JSON_ARRAY_SIZE(THERMAL_PIXELS) + 10200; // Adjusted estimate
    JsonDocument doc; // Defaults to dynamic allocation

    // --- Populate JSON ---
    doc["max_temp"] = maxTemp;
    doc["min_temp"] = minTemp;
    doc["avg_temp"] = avgTemp;

    // Create and populate the "temperatures" array
    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to create JSON array for thermal data (isNull). Check memory.");
        #endif
        return ""; // Return empty string on failure
    }

    for(int i = 0; i < THERMAL_PIXELS; ++i) {
        if (isnan(thermalData[i])) {
             tempArray.add(nullptr); // Represent NaN as null in JSON
        } else {
             tempArray.add(thermalData[i]);
        }
    }

    // Serialize the complete JSON document to a String
    String jsonString;
    size_t written = serializeJson(doc, jsonString);

    if (written == 0) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to serialize thermal JSON data (written 0 bytes).");
        #endif
        return "";
    }

    #ifdef ENABLE_DEBUG_SERIAL
       Serial.printf("[MultipartSender] Generated Thermal JSON String Length: %d\n", jsonString.length());
       Serial.printf("  Calculated Stats: Max=%.2f, Min=%.2f, Avg=%.2f\n", maxTemp, minTemp, avgTemp);
    #endif
    return jsonString;
}

/**
 * @brief Calculates the maximum temperature from the thermal data array.
 */
/* static */ float MultipartDataSender::calculateMaxTemperature(float* thermalData) {
    if (thermalData == nullptr) return -INFINITY; // Handle null input

    float maxTemp = -INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i]) && thermalData[i] > maxTemp) {
            maxTemp = thermalData[i];
        }
    }
    return maxTemp; // Returns -INFINITY if all were NaN or array was empty
}

/**
 * @brief Calculates the minimum temperature from the thermal data array.
 */
/* static */ float MultipartDataSender::calculateMinTemperature(float* thermalData) {
     if (thermalData == nullptr) return INFINITY; // Handle null input

    float minTemp = INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i]) && thermalData[i] < minTemp) {
            minTemp = thermalData[i];
        }
    }
    return minTemp; // Returns INFINITY if all were NaN or array was empty
}

/**
 * @brief Calculates the average temperature from the thermal data array.
 */
/* static */ float MultipartDataSender::calculateAverageTemperature(float* thermalData) {
    if (thermalData == nullptr) return NAN; // Handle null input

    double sumTemp = 0.0;
    int validPixelCount = 0;

    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i])) {
            sumTemp += thermalData[i];
            validPixelCount++;
        }
    }

    if (validPixelCount > 0) {
        return (float)(sumTemp / validPixelCount);
    } else {
        return NAN; // Return Not-a-Number if no valid pixels were found
    }
}


/**
 * @brief Builds the complete multipart/form-data payload as a byte vector.
 */
/* static */ std::vector<uint8_t> MultipartDataSender::buildMultipartPayload(
    const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength
) {
    std::vector<uint8_t> payload;
    size_t estimatedSize = thermalJson.length() + jpegLength + 512; // Estimate size

    try {
        payload.reserve(estimatedSize);
    } catch (const std::bad_alloc& e) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MultipartSender Error] Failed to reserve %d bytes for payload vector: %s\n", estimatedSize, e.what());
        #endif
        return payload; // Return empty vector
    }

    // Helper lambdas
    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };
    auto appendRaw = [&payload](const char* str) {
        payload.insert(payload.end(), str, str + strlen(str));
    };

    // --- Part 1: Thermal Data (JSON) ---
    appendString("--" + boundary + "\r\n");
    appendRaw("Content-Disposition: form-data; name=\"thermal\"\r\n");
    appendRaw("Content-Type: application/json\r\n\r\n");
    appendString(thermalJson);
    appendRaw("\r\n");

    // --- Part 2: Image Data (JPEG) ---
    appendString("--" + boundary + "\r\n");
    appendRaw("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
    appendRaw("Content-Type: image/jpeg\r\n\r\n");
    payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
    appendRaw("\r\n");

    // --- Closing Boundary ---
    appendString("--" + boundary + "--\r\n");

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MultipartSender] Built multipart payload. Total size: %d bytes.\n", payload.size());
    #endif

    return payload;
}

/**
 * @brief Performs the actual HTTP POST request with the multipart payload and retries.
 */
/* static */ int MultipartDataSender::performHttpPost(
    const String& apiUrl,
    const String& accessToken, // Added accessToken
    const String& boundary,
    const std::vector<uint8_t>& payload
) {
    HTTPClient http;
    int httpResponseCode = -16; // Default to a generic client error for this function
    const int maxRetries = 2; // Reduced retries for data sending as an example, can be adjusted

    #ifdef ENABLE_DEBUG_SERIAL
      Serial.printf("[MultipartSender] Initiating HTTP POST request to: %s\n", apiUrl.c_str());
      Serial.printf("  Payload Size: %d bytes\n", (int)payload.size());
    #endif

    http.setReuse(false);
    if (http.begin(apiUrl)) { // For HTTP. For HTTPS, use WiFiClientSecure.
        http.setTimeout(CAPTURE_DATA_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Connection", "close");
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);

        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
              Serial.println(F("[MultipartSender] Warning: Sending capture data without an access token."));
            #endif
        }

        for (int retry = 0; retry < maxRetries; ++retry) {
            #ifdef ENABLE_DEBUG_SERIAL
              if (retry > 0) Serial.printf("  Retrying POST... (Attempt %d/%d)\n", retry + 1, maxRetries);
            #endif

            // Send the payload from the vector's data pointer and size
            httpResponseCode = http.POST(const_cast<uint8_t*>(payload.data()), payload.size());

            if (httpResponseCode > 0) { // Valid HTTP response received
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP Response Code: %d\n", httpResponseCode);
                  String responseBody = http.getString(); // Capture data endpoint often returns 204 No Content
                  if (!responseBody.isEmpty()) Serial.println("  HTTP Response Body: " + responseBody);
                #endif
                // Ensure response body is consumed if not already read for debug
                if (http.getSize() > 0) {
                    http.getString();
                }
                break; // Exit retry loop on valid response
            } else { // HTTP client error (e.g., connection refused, timeout before response)
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP POST failed (Attempt %d/%d), client error: %s (Code: %d)\n",
                                retry + 1, maxRetries, http.errorToString(httpResponseCode).c_str(), httpResponseCode);
                #endif
                if (retry < maxRetries - 1) {
                    delay( (retry + 1) * 1000 ); // Delay before retry (e.g., 1s, 2s)
                }
            }
        } // End retry loop
        http.end(); // Clean up connection
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.printf("[MultipartSender Error] Unable to begin HTTP connection to: %s\n", apiUrl.c_str());
        #endif
        httpResponseCode = -17; // Custom client error: HTTP begin failed
    }
    return httpResponseCode;
}
