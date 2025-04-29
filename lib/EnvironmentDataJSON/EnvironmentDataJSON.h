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
 * Example: `EnvironmentDataJSON::IOEnvironmentData(...)`
 */
class EnvironmentDataJSON {
public:
    /**
     * @brief Constructs a JSON payload and sends environmental data to an API via HTTP POST.
     *
     * This static method takes environmental sensor readings, formats them into a JSON object
     * with the structure `{"light": value, "temperature": value, "humidity": value}`,
     * and then sends this JSON payload as the body of an HTTP POST request to the
     * specified API URL.
     *
     * @param apiUrl The complete URL (String) of the API endpoint that will receive the POST request.
     * Example: "https://api.example.com/data"
     * @param lightLevel The ambient light level reading (float, e.g., in lux).
     * @param temperature The temperature reading (float, e.g., in Celsius).
     * @param humidity The relative humidity reading (float, e.g., in percentage).
     * @return `true` if the HTTP POST request was sent successfully AND the server responded
     * with a success status code (HTTP 200-299). Returns `false` if the connection
     * failed, the request failed to send, or the server responded with an error code.
     */
    static bool IOEnvironmentData(
        const String& apiUrl,
        float lightLevel,
        float temperature,
        float humidity
    );
};

#endif // ENVIRONMENT_DATA_JSON_H