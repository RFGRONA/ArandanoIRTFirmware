/**
 * @file DHT11Sensor.cpp
 * @brief Implements the DHT11Sensor wrapper class methods.
 */
#include "DHT11Sensor.h"
#include <math.h> // Required for isnan()

/**
 * @brief Constructor implementation.
 * Initializes the pin number and the DHT library object for a DHT11 sensor.
 * @param pin The GPIO pin the DHT11 is connected to.
 */
DHT11Sensor::DHT11Sensor(int pin) : 
    _dhtPin(pin), 
    _dht(pin, DHT11) { // Specify DHT11 as the sensor type
    // The _dht object is now constructed and associated with the correct pin and type.
}

/**
 * @brief Initializes the underlying DHT library.
 * This function needs to be called in the setup() routine before reading sensor data.
 */
void DHT11Sensor::begin() {
    _dht.begin();
}

/**
 * @brief Reads temperature from the DHT11 sensor.
 * Includes a check for NaN (Not a Number) to indicate read failure.
 * @return Temperature in Celsius, or NAN if the read fails.
 */
float DHT11Sensor::readTemperature() {
    // Attempt to read temperature from the sensor
    float t = _dht.readTemperature(); // Returns temperature in Celsius or NaN

    // Check if the reading was successful (Adafruit DHT library returns NaN on error)
    if (isnan(t)) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Optional: More specific error for DHT11 if needed
            // Serial.println("[DHT11Sensor] Failed to read temperature!"); 
        #endif
        return NAN; // Return NAN to indicate failure
    } else {
        return t;   // Return the valid temperature reading
    }
}

/**
 * @brief Reads humidity from the DHT11 sensor.
 * Includes a check for NaN (Not a Number) to indicate read failure.
 * @return Humidity in %, or NAN if the read fails.
 */
float DHT11Sensor::readHumidity() {
    // Attempt to read humidity from the sensor
    float h = _dht.readHumidity(); // Returns humidity in % or NaN

    // Check if the reading was successful
    if (isnan(h)) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Optional: More specific error for DHT11 if needed
            // Serial.println("[DHT11Sensor] Failed to read humidity!");
        #endif
        return NAN; // Return NAN to indicate failure
    } else {
        return h;   // Return the valid humidity reading
    }
}