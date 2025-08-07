/**
 * @file DHT22Sensor.h
 * @brief Defines a wrapper class for the DHT22 temperature and humidity sensor.
 *
 * Provides a simple interface to initialize and read temperature and humidity
 * values from a DHT22 sensor connected to a specific GPIO pin, using the
 * Adafruit DHT sensor library.
 */
#ifndef DHT22SENSOR_H
#define DHT22SENSOR_H

#include <Arduino.h> // Standard Arduino definitions
#include <DHT.h>     // Include the Adafruit DHT sensor library

/**
 * @class DHT22Sensor
 * @brief Wrapper class simplifying the use of a DHT22 temperature and humidity sensor.
 *
 * This class encapsulates the Adafruit DHT library object for a DHT22 sensor,
 * providing straightforward methods to initialize the sensor and read measurements.
 */
class DHT22Sensor {
public:
    /**
     * @brief Constructor for the DHT22 sensor wrapper.
     * @param pin The GPIO pin number where the DHT22 sensor's data line is connected.
     */
    DHT22Sensor(int pin);

    /**
     * @brief Initializes the DHT sensor library communication.
     *
     * This must be called once (e.g., in `setup()`) before attempting to read
     * values from the sensor.
     */
    void begin();

    /**
     * @brief Reads the temperature from the sensor.
     *
     * Attempts to read the current temperature value from the DHT22 sensor.
     * @return The temperature in degrees Celsius ($^{\circ}C$). Returns `NAN` (Not a Number)
     * if the read operation failed (e.g., communication error, checksum failure).
     */
    float readTemperature();

    /**
     * @brief Reads the relative humidity from the sensor.
     *
     * Attempts to read the current relative humidity value from the DHT22 sensor.
     * @return The relative humidity as a percentage (%). Returns `NAN` (Not a Number)
     * if the read operation failed (e.g., communication error, checksum failure).
     */
    float readHumidity();

private:
    int dhtPin; ///< @brief GPIO pin number connected to the sensor's data line.
    DHT dht;    ///< @brief Instance of the underlying Adafruit DHT library object configured for DHT22.
};

#endif // DHT22SENSOR_H