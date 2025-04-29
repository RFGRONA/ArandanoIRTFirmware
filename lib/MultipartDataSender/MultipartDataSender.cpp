/**
 * @file MultipartDataSender.cpp
 * @brief Implements the MultipartDataSender utility class for sending combined thermal (JSON) and image (JPEG) data via HTTP POST.
 */
#include "MultipartDataSender.h"
#include <ArduinoJson.h> // For creating the JSON part
#include <vector>        // Using std::vector to dynamically build the byte payload
#include <esp_random.h>  // For generating a random boundary component

// Define ENABLE_DEBUG_SERIAL in your build flags or main sketch to enable Serial prints
// #define ENABLE_DEBUG_SERIAL

/**
 * @brief Sends thermal data and a JPEG image via HTTP POST using multipart/form-data encoding.
 * This is the main public static method that orchestrates the process.
 */
/* static */ bool MultipartDataSender::IOThermalAndImageData(
    const String& apiUrl,
    float* thermalData,
    uint8_t* jpegImage,
    size_t jpegLength
) {
    // --- Step 1: Validate Input Data ---
    if (thermalData == nullptr || jpegImage == nullptr || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Invalid input: Null pointer or zero length image provided.");
        #endif
        return false;
    }

    // --- Step 2: Create JSON Payload for Thermal Data ---
    String thermalJsonString = createThermalJson(thermalData);
    if (thermalJsonString.length() == 0) {
        // Error should have been logged within createThermalJson if ENABLE_DEBUG_SERIAL is defined
        return false; // Exit if JSON creation failed
    }

    // --- Step 3: Generate a Unique Boundary String ---
    // Using esp_random() for a reasonably unique boundary part.
    String boundary = "----WebKitFormBoundaryESP32" + String(esp_random());

    // --- Step 4: Build the Complete Multipart Payload ---
    std::vector<uint8_t> payload = buildMultipartPayload(boundary, thermalJsonString, jpegImage, jpegLength);
    if (payload.empty()) {
        // Error likely related to memory allocation within buildMultipartPayload
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to build multipart payload (payload vector is empty).");
        #endif
        return false; // Exit if payload building failed
    }

    // --- Step 5: Perform the HTTP POST Request with Retries ---
    return performHttpPost(apiUrl, boundary, payload);
}

// --- Helper Function Implementations ---

/**
 * @brief Creates a JSON object string `{"temperatures": [...]}` from the thermal data array.
 */
/* static */ String MultipartDataSender::createThermalJson(float* thermalData) {
    // Using dynamic JsonDocument. Capacity calculation is an estimate.
    // If memory is extremely tight or JSON structure is fixed, StaticJsonDocument might be considered.
    // Required capacity: JSON object framing + key "temperatures" + JSON array framing + 768 floats (comma-separated)
    // Rough estimate: JSON_OBJECT_SIZE(1) + 15 ("temperatures":[]) + JSON_ARRAY_SIZE(768) + 768 * (avg_float_len + 1_comma)
    // A simpler large-enough estimate:
    const size_t capacity = JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(768) + 10000; // Generous estimate for 768 floats
    JsonDocument doc; // Defaults to dynamic allocation

    // Create the root object and add the "temperatures" array
    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        // This usually happens if memory allocation for the JsonDocument fails.
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to create JSON array for thermal data (isNull). Check memory.");
        #endif
        return ""; // Return empty string on failure
    }

    // Populate the JSON array with temperature values
    const int totalPixels = 32 * 24;
    for(int i = 0; i < totalPixels; ++i) {
        // Add temperature value. ArduinoJson handles float formatting.
        // Consider adding checks here if thermalData can contain NaN or invalid values:
        // if (isnan(thermalData[i])) { tempArray.add(nullptr); } else { tempArray.add(thermalData[i]); }
        tempArray.add(thermalData[i]);
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
       // Serial.println("Generated Thermal JSON: " + jsonString); // Can be very long!
       Serial.printf("[MultipartSender] Generated Thermal JSON String Length: %d\n", jsonString.length());
    #endif
    return jsonString;
}

/**
 * @brief Builds the complete multipart/form-data payload as a byte vector.
 */
/* static */ std::vector<uint8_t> MultipartDataSender::buildMultipartPayload(
    const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength
) {
    std::vector<uint8_t> payload;

    // Estimate required size to potentially avoid reallocations. Be generous.
    // Size = Headers/Boundaries (~512 bytes) + JSON length + JPEG length
    size_t estimatedSize = thermalJson.length() + jpegLength + 512;
    try {
        payload.reserve(estimatedSize);
    } catch (const std::bad_alloc& e) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MultipartSender Error] Failed to reserve %d bytes for payload vector: %s\n", estimatedSize, e.what());
        #endif
        return payload; // Return empty vector
    }

    // Helper lambda to append String data efficiently to the byte vector
    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };
     // Helper lambda to append raw C-string data
    auto appendRaw = [&payload](const char* str) {
        payload.insert(payload.end(), str, str + strlen(str));
    };

    // --- Part 1: Thermal Data (JSON) ---
    appendString("--" + boundary + "\r\n");
    appendRaw("Content-Disposition: form-data; name=\"thermal\"\r\n"); // Field name "thermal"
    appendRaw("Content-Type: application/json\r\n\r\n"); // Specify JSON content type
    appendString(thermalJson); // Append the JSON data
    appendRaw("\r\n"); // End of part content

    // --- Part 2: Image Data (JPEG) ---
    appendString("--" + boundary + "\r\n");
    // Field name "image", filename "camera.jpg" (server might use this)
    appendRaw("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
    appendRaw("Content-Type: image/jpeg\r\n\r\n"); // Specify JPEG content type
    // Append binary JPEG image data directly
    payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
    appendRaw("\r\n"); // End of part content

    // --- Closing Boundary ---
    appendString("--" + boundary + "--\r\n"); // Final boundary indicates end of message

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MultipartSender] Built multipart payload. Total size: %d bytes.\n", payload.size());
    #endif

    return payload; // Return the fully constructed payload
}

/**
 * @brief Performs the actual HTTP POST request with the multipart payload and retries.
 */
/* static */ bool MultipartDataSender::performHttpPost(
    const String& apiUrl, const String& boundary, const std::vector<uint8_t>& payload
) {
    HTTPClient http;
    bool success = false;
    const int maxRetries = 3; // Number of times to retry the POST operation on failure

    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("[MultipartSender] Initiating HTTP POST request to: " + apiUrl);
      Serial.printf("  Payload Size: %d bytes\n", payload.size());
    #endif

    // Configure HTTP client. Use WiFiClient for HTTP, WiFiClientSecure for HTTPS.
    // Note: begin() can fail if URL is invalid.
    if (http.begin(apiUrl)) {

        // --- Set Necessary HTTP Headers ---
        // Content-Type is crucial for multipart requests, includes the boundary
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        // Content-Length is often required by servers
        http.addHeader("Content-Length", String(payload.size()));
        // Add any other required headers, e.g., Authorization for protected APIs
        // http.addHeader("Authorization", "Bearer YOUR_AUTH_TOKEN");

        int httpResponseCode = -1; // Initialize with an invalid code

        // --- Attempt POST Request with Retries ---
        for (int retry = 0; retry < maxRetries; ++retry) {
            #ifdef ENABLE_DEBUG_SERIAL
              if (retry > 0) Serial.printf("  Retrying POST... (Attempt %d/%d)\n", retry + 1, maxRetries);
            #endif

            // Perform the POST request sending the raw byte payload.
            // Use const_cast as HTTPClient::POST expects uint8_t* but shouldn't modify it.
            httpResponseCode = http.POST(const_cast<uint8_t*>(payload.data()), payload.size());

            if (httpResponseCode > 0) {
                // Received a valid HTTP response code from the server (e.g., 200, 404, 500)
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP Response Code: %d\n", httpResponseCode);
                #endif
                // It's good practice to read the response body, even if not used,
                // to properly clear the connection buffer.
                String responseBody = http.getString();
                #ifdef ENABLE_DEBUG_SERIAL
                  // Avoid printing very large response bodies unless necessary
                  if (responseBody.length() < 256) {
                     Serial.println("  HTTP Response Body: " + responseBody);
                  } else {
                     Serial.printf("  HTTP Response Body: (Truncated - %d bytes)\n", responseBody.length());
                  }
                #endif
                break; // Exit the retry loop as we got a definitive response from the server
            } else {
                // HTTP request itself failed (e.g., connection refused, timeout, DNS error)
                // httpResponseCode will be negative or zero.
                #ifdef ENABLE_DEBUG_SERIAL
                  Serial.printf("  HTTP POST failed (Attempt %d/%d), client error: %s\n",
                                retry + 1, maxRetries, http.errorToString(httpResponseCode).c_str());
                #endif
                // Wait a bit before retrying connection/send errors
                delay( (retry + 1) * 500 ); // Increasing delay: 500ms, 1000ms, 1500ms
            }
        } // End retry loop

        // --- Check Final Response Code ---
        // Evaluate success based *only* on the final HTTP response code received.
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            #ifdef ENABLE_DEBUG_SERIAL
              Serial.println("[MultipartSender] Request successful (HTTP 2xx).");
            #endif
            success = true;
        } else {
             #ifdef ENABLE_DEBUG_SERIAL
              if (httpResponseCode > 0) {
                 Serial.printf("[MultipartSender Error] Request failed, server returned non-2xx status: %d\n", httpResponseCode);
              } else {
                 Serial.println("[MultipartSender Error] Request failed, no valid response received after retries.");
              }
            #endif
             success = false;
        }

        // --- Clean up HTTP Connection ---
        http.end();

    } else {
        // http.begin(apiUrl) failed. Likely an invalid URL or DNS issue.
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Unable to begin HTTP connection to: " + apiUrl);
        #endif
        success = false;
    }

    return success; // Return the final success status
}