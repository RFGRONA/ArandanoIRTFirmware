/**
 * @file EnvironmentDataJSON.h
 * @brief Defines a utility class to format and send environmental sensor data
 * as JSON via HTTP POST requests.
 */
#ifndef ENVIRONMENT_DATA_JSON_H
#define ENVIRONMENT_DATA_JSON_H

#include <ArduinoJson.h> // Required for JSON formatting (using ArduinoJson v6+)
#include <HTTPClient.h>  // Required for making HTTP requests on ESP32

/**
 * @class EnvironmentDataJSON
 * @brief Utility class containing a static method for sending environmental data.
 */
class EnvironmentDataJSON {
public:
    /**
     * @brief Constructs a JSON payload and sends environmental data to an API via HTTP POST.
     *
     * This static method takes environmental sensor readings, formats them into a JSON object:
     * `{"light": value, "temperature": value, "humidity": value, "pressure": value}`,
     * and sends this JSON payload as the body of an HTTP POST request.
     *
     * @param fullEnvDataUrl The complete URL (String) of the API endpoint.
     * @param accessToken The access token (String) for Authorization header.
     * @param timestamp The time of capture in ISO 8601 format (String).
     * @param lightLevel The ambient light level reading (float, lux).
     * @param temperature The temperature reading (float, °C).
     * @param humidity The relative humidity reading (float, %).
     * @param pressure The barometric pressure reading (float, hPa). 
     * @return The HTTP status code returned by the server.
     */
    static int IOEnvironmentData(
        const String& fullEnvDataUrl,
        const String& accessToken,
        const String& timestamp,
        float lightLevel,
        float temperature,
        float humidity,
        float pressure 
    );
};

#endif // ENVIRONMENT_DATA_JSON_H