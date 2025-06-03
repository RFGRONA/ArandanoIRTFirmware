/**
 * @file DHT11Sensor.h
 * @brief Defines a wrapper class for the DHT11 temperature and humidity sensor.
 *
 * Provides a simple interface to initialize and read temperature and humidity
 * values from a DHT11 sensor connected to a specific GPIO pin, using the
 * Adafruit DHT sensor library. This class is specifically for the DHT11 sensor type.
 */
#ifndef DHT11SENSOR_H
#define DHT11SENSOR_H

#include <Arduino.h> // Standard Arduino definitions
#include <DHT.h>     // Include the Adafruit DHT sensor library

/**
 * @class DHT11Sensor
 * @brief Wrapper class simplifying the use of a DHT11 temperature and humidity sensor.
 *
 * This class encapsulates the Adafruit DHT library object for a DHT11 sensor,
 * providing straightforward methods to initialize the sensor and read measurements.
 * It handles the specific sensor type DHT11.
 */
class DHT11Sensor {
public:
    /**
     * @brief Constructor for the DHT11 sensor wrapper.
     * @param pin The GPIO pin number where the DHT11 sensor's data line is connected.
     */
    DHT11Sensor(int pin);

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
     * Attempts to read the current temperature value from the DHT11 sensor.
     * @return The temperature in degrees Celsius ($^{\circ}C$). Returns `NAN` (Not a Number)
     * if the read operation failed (e.g., communication error, checksum failure).
     */
    float readTemperature();

    /**
     * @brief Reads the relative humidity from the sensor.
     *
     * Attempts to read the current relative humidity value from the DHT11 sensor.
     * @return The relative humidity as a percentage (%). Returns `NAN` (Not a Number)
     * if the read operation failed (e.g., communication error, checksum failure).
     */
    float readHumidity();

private:
    int _dhtPin; ///< @brief GPIO pin number connected to the sensor's data line.
    DHT _dht;    ///< @brief Instance of the underlying Adafruit DHT library object configured for DHT11.
};

#endif // DHT11SENSOR_H