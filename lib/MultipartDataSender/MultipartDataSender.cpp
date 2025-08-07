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
    // CORRECTION: Thermal data is mandatory, but image data is now optional.
    if (thermalData == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Invalid input: Null pointer for thermal data."));
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
    if (thermalJsonString.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Failed to create thermal JSON."));
        #endif
        return -14; // Custom client error: JSON creation failed
    }

    // --- Step 3: Generate a Unique Boundary String ---
    String boundary = "----WebKitFormBoundaryESP32-" + String(esp_random(), HEX) + String(esp_random(), HEX);

    // --- Step 4: Build the Complete Multipart Payload ---
    // CORRECTION: Pass the image data to the payload builder, which will handle if it's null.
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
    // Calculate statistics first
    float maxTemp = calculateMaxTemperature(thermalData);
    float minTemp = calculateMinTemperature(thermalData);
    float avgTemp = calculateAverageTemperature(thermalData);

    if (isinf(maxTemp) || isinf(minTemp) || isnan(avgTemp)) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Warning] Thermal stats calculation resulted in Inf/NaN.");
         #endif
         return "";
    }

    JsonDocument doc;

    doc["max_temp"] = maxTemp;
    doc["min_temp"] = minTemp;
    doc["avg_temp"] = avgTemp;

    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to create JSON array for thermal data.");
        #endif
        return "";
    }

    for(int i = 0; i < THERMAL_PIXELS; ++i) {
        if (isnan(thermalData[i])) {
             tempArray.add(nullptr);
        } else {
             tempArray.add(thermalData[i]);
        }
    }

    String jsonString;
    size_t written = serializeJson(doc, jsonString);

    if (written == 0) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to serialize thermal JSON data.");
        #endif
        return "";
    }

    #ifdef ENABLE_DEBUG_SERIAL
       Serial.printf("[MultipartSender] Generated Thermal JSON String Length: %d\n", jsonString.length());
    #endif
    return jsonString;
}

// ... (calculateMax/Min/AverageTemperature functions remain unchanged) ...
float MultipartDataSender::calculateMaxTemperature(float* thermalData) {
    if (thermalData == nullptr) return -INFINITY;
    float maxTemp = -INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i]) && thermalData[i] > maxTemp) {
            maxTemp = thermalData[i];
        }
    }
    return maxTemp;
}

float MultipartDataSender::calculateMinTemperature(float* thermalData) {
     if (thermalData == nullptr) return INFINITY;
    float minTemp = INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i]) && thermalData[i] < minTemp) {
            minTemp = thermalData[i];
        }
    }
    return minTemp;
}

float MultipartDataSender::calculateAverageTemperature(float* thermalData) {
    if (thermalData == nullptr) return NAN;
    double sumTemp = 0.0;
    int validPixelCount = 0;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        if (!isnan(thermalData[i])) {
            sumTemp += thermalData[i];
            validPixelCount++;
        }
    }
    return (validPixelCount > 0) ? (float)(sumTemp / validPixelCount) : NAN;
}


/**
 * @brief Builds the complete multipart/form-data payload as a byte vector.
 */
/* static */ std::vector<uint8_t> MultipartDataSender::buildMultipartPayload(
    const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength
) {
    std::vector<uint8_t> payload;
    // CORRECTION: Estimate size based on whether the image is present.
    size_t estimatedSize = thermalJson.length() + (jpegImage ? jpegLength : 0) + 512;
    payload.reserve(estimatedSize);

    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };
    auto appendRaw = [&payload](const char* str) {
        payload.insert(payload.end(), str, str + strlen(str));
    };

    // --- Part 1: Thermal Data (JSON) --- (Always included)
    appendString("--" + boundary + "\r\n");
    appendRaw("Content-Disposition: form-data; name=\"thermal\"\r\n");
    appendRaw("Content-Type: application/json\r\n\r\n");
    appendString(thermalJson);
    appendRaw("\r\n");

    // --- Part 2: Image Data (JPEG) --- (CORRECTION: Conditionally included)
    if (jpegImage != nullptr && jpegLength > 0) {
        appendString("--" + boundary + "\r\n");
        appendRaw("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
        appendRaw("Content-Type: image/jpeg\r\n\r\n");
        payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
        appendRaw("\r\n");
    }

    // --- Closing Boundary ---
    appendString("--" + boundary + "--\r\n");

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MultipartSender] Built multipart payload. Total size: %d bytes.\n", payload.size());
    #endif

    return payload;
}

/**
 * @brief Performs the actual HTTP POST request with the multipart payload.
 */
/* static */ int MultipartDataSender::performHttpPost(
    const String& apiUrl,
    const String& accessToken,
    const String& boundary,
    const std::vector<uint8_t>& payload
) {
    HTTPClient http;
    int httpResponseCode = -16;
    
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.printf("[MultipartSender] Initiating HTTP POST request to: %s\n", apiUrl.c_str());
    #endif

    http.setReuse(false);
    if (http.begin(apiUrl)) {
        http.setTimeout(CAPTURE_DATA_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Connection", "close");
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        }
        
        httpResponseCode = http.POST(const_cast<uint8_t*>(payload.data()), payload.size());

        #ifdef ENABLE_DEBUG_SERIAL
            if (httpResponseCode > 0) {
                Serial.printf("  HTTP Response Code: %d\n", httpResponseCode);
            } else {
                Serial.printf("  HTTP POST failed, client error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            }
        #endif
        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.printf("[MultipartSender Error] Unable to begin HTTP connection to: %s\n", apiUrl.c_str());
        #endif
        httpResponseCode = -17;
    }
    return httpResponseCode;
}
