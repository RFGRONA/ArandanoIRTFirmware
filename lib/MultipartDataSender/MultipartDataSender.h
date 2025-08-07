/**
 * @file MultipartDataSender.h
 * @brief Defines a utility class to send thermal data (JSON) and image data (JPEG)
 * as a multipart/form-data POST request.
 *
 * Provides a static method to package thermal sensor readings (including raw data,
 * max, min, and average temperature) and a captured camera image (as JPEG binary data)
 * into a single HTTP POST request suitable for APIs expecting multipart/form-data.
 * Uses ESP32 HTTPClient and ArduinoJson libraries.
 */
#ifndef MULTIPART_DATA_SENDER_H
#define MULTIPART_DATA_SENDER_H

#include <HTTPClient.h>      // Required for making HTTP requests on ESP32
#include <WiFi.h>            // Used indirectly by HTTPClient for network connection
#include <ArduinoJson.h>     // Required for creating the thermal data JSON payload
#include <vector>            // Required for std::vector used in implementation
#include <math.h>            // Required for INFINITY, NAN, isnan

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

    // --- Constants ---
    static const int THERMAL_WIDTH = 32; ///< Width of the thermal sensor array.
    static const int THERMAL_HEIGHT = 24; ///< Height of the thermal sensor array.
    static const int THERMAL_PIXELS = THERMAL_WIDTH * THERMAL_HEIGHT; ///< Total number of thermal sensor pixels.

    /**
     * @brief Sends thermal sensor data and a JPEG image via HTTP POST using multipart/form-data encoding.
     *
     * Constructs a multipart payload containing two distinct parts:
     * 1. Part name 'thermal': Contains the thermal data formatted as a JSON object string,
     * including fields for "max_temp", "min_temp", "avg_temp", and the raw "temperatures" array.
     * Example: `{"max_temp": 35.2, "min_temp": 28.1, "avg_temp": 31.5, "temperatures": [...]}`.
     * 2. Part name 'image': Contains the raw binary JPEG image data, with a default
     * filename "camera.jpg".
     *
     * This combined payload is then sent via HTTP POST to the specified API endpoint.
     * The function includes basic retry logic for the HTTP POST operation.
     *
     * @param fullCaptureDataUrl The complete URL (String) of the API endpoint for capture data.
     * @param accessToken The access token (String) for `Authorization: Device <token>` header.
     * @param thermalData Pointer to the float array (768 elements) of thermal readings.
     * @param jpegImage Pointer to the byte array (uint8_t*) of JPEG image data.
     * @param jpegLength The size (size_t) of the JPEG image data in bytes.
     * @return The HTTP status code returned by the server. Returns a negative value
     * if a client-side error occurred (e.g., invalid input, payload build error, HTTP failure).
     */
     static int IOThermalAndImageData( 
        const String& fullCaptureDataUrl,
        const String& accessToken,
        float* thermalData,
        uint8_t* jpegImage,
        size_t jpegLength
    );

    /**
     * @brief Creates a JSON object string containing thermal statistics and the raw data array.
     * Formats the data including max, min, average, and the full temperature array.
     * @param thermalData Pointer to the float array (THERMAL_PIXELS elements) of temperatures.
     * @return String containing the formatted JSON payload. Returns an empty string on allocation or calculation failure.
     */
    static String createThermalJson(float* thermalData);


    // --- Thermal Data Calculation Helpers ---

    /**
     * @brief Calculates the maximum temperature from the thermal data array.
     * Ignores NaN values during calculation.
     * @param thermalData Pointer to the float array (THERMAL_PIXELS elements) of temperatures.
     * @return The maximum valid temperature found in the array. Returns -INFINITY if all values are NaN or the array is empty/invalid.
     */
    static float calculateMaxTemperature(float* thermalData);

    /**
     * @brief Calculates the minimum temperature from the thermal data array.
     * Ignores NaN values during calculation.
     * @param thermalData Pointer to the float array (THERMAL_PIXELS elements) of temperatures.
     * @return The minimum valid temperature found in the array. Returns INFINITY if all values are NaN or the array is empty/invalid.
     */
    static float calculateMinTemperature(float* thermalData);

    /**
     * @brief Calculates the average temperature from the thermal data array.
     * Ignores NaN values during calculation. The average is calculated based on the count of valid (non-NaN) pixels.
     * @param thermalData Pointer to the float array (THERMAL_PIXELS elements) of temperatures.
     * @return The average temperature of valid pixels. Returns NAN if all values are NaN or the array is empty/invalid.
     */
    static float calculateAverageTemperature(float* thermalData);

private:

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
     * @param accessToken The access token (String) for `Authorization: Device <token>` header.
     * @param boundary The multipart boundary string (required for the Content-Type header).
     * @param payload The complete multipart payload as a byte vector (`std::vector<uint8_t>`).
     * @return `true` if the POST request receives an HTTP 2xx success status code within the retry limit, `false` otherwise.
     */
    static int performHttpPost( // Return type changed to int
        const String& apiUrl,
        const String& accessToken, // Added accessToken
        const String& boundary,
        const std::vector<uint8_t>& payload
    );

};

#endif // MULTIPART_DATA_SENDER_H
