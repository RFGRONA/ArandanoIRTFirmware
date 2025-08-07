/**
 * @file EnvironmentDataJSON.h
 * @brief Defines a utility class to format and send environmental sensor data
 * as JSON via HTTP POST requests.
 *
 * This class provides a static method to simplify the process of taking sensor
 * readings (light, temperature, humidity), formatting them into a standard JSON
 * object, and sending this data to a specified API endpoint using the ESP32
 * HTTPClient library.
 */
#ifndef ENVIRONMENT_DATA_JSON_H
#define ENVIRONMENT_DATA_JSON_H

#include <ArduinoJson.h> // Required for JSON formatting (using ArduinoJson v6+)
#include <HTTPClient.h>  // Required for making HTTP requests on ESP32

/**
 * @class EnvironmentDataJSON
 * @brief Utility class containing a static method for sending environmental data.
 *
 * As this class only contains a static method, you don't need to instantiate it.
 * Call the method directly using the class name scope resolution operator (::).
 */
class EnvironmentDataJSON {
public:
    /**
     * @brief Constructs a JSON payload and sends environmental data to an API via HTTP POST.
     *
     * This static method takes environmental sensor readings, formats them into a JSON object:
     * `{"light": value, "temperature": value, "humidity": value}`,
     * and sends this JSON payload as the body of an HTTP POST request to the
     * specified API URL, including an authorization token.
     *
     * @param fullEnvDataUrl The complete URL (String) of the API endpoint for environmental data.
     * @param accessToken The access token (String) for `Authorization: Device <token>` header.
     * @param lightLevel The ambient light level reading (float).
     * @param temperature The temperature reading (float).
     * @param humidity The relative humidity reading (float).
     * @return The HTTP status code returned by the server. Returns a negative value
     * if a client-side error occurred (e.g., connection failure, HTTPClient error).
     */
    static int IOEnvironmentData( // Return type changed to int
        const String& fullEnvDataUrl,
        const String& accessToken,
        float lightLevel,
        float temperature,
        float humidity
    );
};

#endif // ENVIRONMENT_DATA_JSON_H