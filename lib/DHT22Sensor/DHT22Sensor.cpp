#include "DHT22Sensor.h"

// Constructor implementation - Initializes the pin number and the DHT library object.
// The DHT object is initialized with the specified pin and sensor type (DHT22).
DHT22Sensor::DHT22Sensor(int pin) : dhtPin(pin), dht(pin, DHT22) {
    // Nothing else to do in the constructor body
}

// Initializes the underlying DHT library.
void DHT22Sensor::begin() {
    dht.begin();
}

// Reads temperature by calling the underlying library's method.
// Includes a check for NaN (Not a Number) to indicate read failure.
float DHT22Sensor::readTemperature() {
    float t = dht.readTemperature();
    // Check if the reading was successful (DHT library returns NaN on error)
    if (isnan(t)) {
        return NAN; // Return NAN to indicate failure
    } else {
        return t;   // Return the valid temperature reading
    }
}

// Reads humidity by calling the underlying library's method.
// Includes a check for NaN (Not a Number) to indicate read failure.
float DHT22Sensor::readHumidity() {
    float h = dht.readHumidity();
    // Check if the reading was successful (DHT library returns NaN on error)
    if (isnan(h)) {
        return NAN; // Return NAN to indicate failure
    } else {
        return h;   // Return the valid humidity reading
    }
}