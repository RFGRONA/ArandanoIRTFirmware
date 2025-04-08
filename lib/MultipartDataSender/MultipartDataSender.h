#ifndef MULTIPART_DATA_SENDER_H
#define MULTIPART_DATA_SENDER_H

#include <HTTPClient.h>      // For making HTTP requests
#include <WiFi.h>            // Used indirectly by HTTPClient
#include <ArduinoJson.h>     // For creating the thermal data JSON payload

/**
 * @file MultipartDataSender.h
 * @brief Utility class to send thermal data (JSON) and image data (JPEG) as a multipart/form-data POST request.
 *
 * Provides a static method to package thermal sensor readings (as a JSON array)
 * and a captured camera image (as JPEG binary data) into a single HTTP POST
 * request suitable for APIs expecting multipart/form-data.
 */
class MultipartDataSender {
public:
    /**
     * @brief Sends thermal data and a JPEG image via HTTP POST using multipart/form-data encoding.
     *
     * Constructs a multipart payload with two parts:
     * 1. 'thermal': Contains a JSON string representing the thermal data array.
     * 2. 'image': Contains the raw binary JPEG image data.
     * Sends this payload to the specified API endpoint. Includes basic retry logic.
     *
     * @param apiUrl The full URL of the API endpoint expecting the multipart POST request.
     * @param thermalData Pointer to the float array containing 768 (32x24) thermal readings (°C).
     * @param jpegImage Pointer to the byte array containing the JPEG image data.
     * @param jpegLength The size of the JPEG image data in bytes.
     * @return True if the HTTP POST request returns a success code (200-299) after retries, False otherwise.
     */
    static bool IOThermalAndImageData(
        const String& apiUrl,
        float* thermalData,
        uint8_t* jpegImage,
        size_t jpegLength
    );
};

#endif // MULTIPART_DATA_SENDER_H