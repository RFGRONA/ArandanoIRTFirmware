/**
 * @file MultipartDataSender.h
 * @brief Defines a utility class to send thermal data (JSON) and image data (JPEG)
 * as a multipart/form-data POST request.
 *
 * Provides a static method to package thermal sensor readings (as a JSON object
 * containing an array) and a captured camera image (as JPEG binary data) into a
 * single HTTP POST request suitable for APIs expecting multipart/form-data.
 * Uses ESP32 HTTPClient and ArduinoJson libraries.
 */
#ifndef MULTIPART_DATA_SENDER_H
#define MULTIPART_DATA_SENDER_H

#include <HTTPClient.h>      // Required for making HTTP requests on ESP32
#include <WiFi.h>            // Used indirectly by HTTPClient for network connection
#include <ArduinoJson.h>     // Required for creating the thermal data JSON payload
#include <vector>            // Required for std::vector used in implementation

/**
 * @class MultipartDataSender
 * @brief Utility class containing a static method for sending combined thermal and image data.
 *
 * This class handles the complexities of formatting a multipart/form-data request,
 * including setting correct headers and boundaries. As it only contains static methods,
 * it does not need to be instantiated.
 */
class MultipartDataSender {
public:
    /**
     * @brief Sends thermal sensor data and a JPEG image via HTTP POST using multipart/form-data encoding.
     *
     * Constructs a multipart payload containing two distinct parts:
     * 1. Part name 'thermal': Contains the thermal data formatted as a JSON object string,
     * specifically `{"temperatures": [temp1, temp2, ..., temp768]}`.
     * 2. Part name 'image': Contains the raw binary JPEG image data, with a default
     * filename "camera.jpg".
     *
     * This combined payload is then sent via HTTP POST to the specified API endpoint.
     * The function includes basic retry logic for the HTTP POST operation.
     *
     * @param apiUrl The full URL (String) of the API endpoint expecting the multipart POST request.
     * @param thermalData Pointer to the float array containing 768 (32x24) thermal readings in degrees Celsius ($^{\circ}C$).
     * @param jpegImage Pointer to the byte array (uint8_t*) containing the raw JPEG image data.
     * @param jpegLength The size (size_t) of the JPEG image data in bytes.
     * @return `true` if the HTTP POST request was successfully sent AND the server responded
     * with a success status code (HTTP 200-299) within the retry limit. Returns `false`
     * otherwise (e.g., invalid input, JSON creation error, payload build error, HTTP connection failure,
     * HTTP send error, or non-2xx server response).
     */
    static bool IOThermalAndImageData(
        const String& apiUrl,
        float* thermalData,
        uint8_t* jpegImage,
        size_t jpegLength
    );

private:
    /**
     * @brief Creates a JSON object string containing the thermal data array.
     * Formats the data as `{"temperatures": [val1, val2, ...]}`.
     * @param thermalData Pointer to the float array (32x24 = 768 elements) of temperatures.
     * @return String containing the formatted JSON payload. Returns an empty string on allocation failure.
     */
    static String createThermalJson(float* thermalData);

    /**
     * @brief Constructs the complete multipart/form-data payload as a byte vector.
     * Assembles the headers, boundaries, JSON data, and binary image data into a single byte stream.
     * @param boundary The unique boundary string used to separate parts in the multipart request.
     * @param thermalJson The JSON string (created by `createThermalJson`) representing the thermal data.
     * @param jpegImage Pointer to the buffer containing the raw JPEG image data.
     * @param jpegLength Size of the JPEG data in bytes.
     * @return `std::vector<uint8_t>` containing the fully formatted multipart payload. Returns an empty vector on error (e.g., memory allocation).
     */
    static std::vector<uint8_t> buildMultipartPayload(const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength);

    /**
     * @brief Performs the actual HTTP POST request, sending the constructed payload with retries.
     * Handles setting the appropriate `Content-Type` and `Content-Length` headers.
     * @param apiUrl The target URL for the POST request.
     * @param boundary The multipart boundary string (required for the Content-Type header).
     * @param payload The complete multipart payload as a byte vector (`std::vector<uint8_t>`).
     * @return `true` if the POST request receives an HTTP 2xx success status code within the retry limit, `false` otherwise.
     */
    static bool performHttpPost(const String& apiUrl, const String& boundary, const std::vector<uint8_t>& payload);
};

#endif // MULTIPART_DATA_SENDER_H