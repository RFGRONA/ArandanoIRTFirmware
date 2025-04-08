#ifndef ENVIRONMENT_DATA_JSON_H
#define ENVIRONMENT_DATA_JSON_H

#include <ArduinoJson.h> // Using ArduinoJson library
#include <HTTPClient.h>  // Using ESP32 HTTPClient library

/**
 * @file EnvironmentDataJSON.h
 * @brief Utility class to format and send environmental data as JSON via HTTP POST.
 *
 * This class provides a static method to simplify sending sensor readings
 * (light, temperature, humidity) to a specified API endpoint.
 */
class EnvironmentDataJSON {
public:
    /**
     * @brief Constructs a JSON payload and sends environmental data to an API via HTTP POST.
     *
     * Takes sensor readings, formats them into a JSON object like:
     * `{"light": value, "temperature": value, "humidity": value}`,
     * and sends this payload to the provided URL.
     *
     * @param apiUrl The full URL of the API endpoint expecting the POST request.
     * @param lightLevel The ambient light level reading (e.g., in lux).
     * @param temperature The temperature reading (e.g., in Celsius).
     * @param humidity The relative humidity reading (e.g., in %).
     * @return True if the HTTP POST request returns a success code (200-299), False otherwise.
     */
    static bool IOEnvironmentData(
        const String& apiUrl,
        float lightLevel,
        float temperature,
        float humidity
    );
};

#endif // ENVIRONMENT_DATA_JSON_H