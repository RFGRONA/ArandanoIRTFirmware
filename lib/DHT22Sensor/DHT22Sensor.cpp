/**
 * @file DHT22Sensor.cpp
 * @brief Implements the DHT22Sensor wrapper class methods.
 */
#include "DHT22Sensor.h"
#include <math.h> // Required for isnan()

// Constructor implementation - Initializes the pin number and the DHT library object.
// The DHT object is initialized with the specified pin and sensor type (DHT22)
// via the member initializer list.
DHT22Sensor::DHT22Sensor(int pin) : dhtPin(pin), dht(pin, DHT22) {
    // The dht object is now constructed and associated with the correct pin and type.
}

// Initializes the underlying DHT library.
// This function needs to be called before reading sensor data.
void DHT22Sensor::begin() {
    dht.begin();
}

// Reads temperature by calling the underlying library's method.
// Includes a check for NaN (Not a Number) to indicate read failure.
float DHT22Sensor::readTemperature() {
    // Attempt to read temperature from the sensor
    float t = dht.readTemperature(); // Returns temperature in Celsius or NaN

    // Check if the reading was successful (Adafruit DHT library returns NaN on error)
    if (isnan(t)) {
        // Optional: Log an error message here if needed
        // Serial.println("Failed to read temperature from DHT sensor!");
        return NAN; // Return NAN to indicate failure
    } else {
        return t;   // Return the valid temperature reading
    }
}

// Reads humidity by calling the underlying library's method.
// Includes a check for NaN (Not a Number) to indicate read failure.
float DHT22Sensor::readHumidity() {
    // Attempt to read humidity from the sensor
    float h = dht.readHumidity(); // Returns humidity in % or NaN

    // Check if the reading was successful (Adafruit DHT library returns NaN on error)
    if (isnan(h)) {
        // Optional: Log an error message here if needed
        // Serial.println("Failed to read humidity from DHT sensor!");
        return NAN; // Return NAN to indicate failure
    } else {
        return h;   // Return the valid humidity reading
    }
}