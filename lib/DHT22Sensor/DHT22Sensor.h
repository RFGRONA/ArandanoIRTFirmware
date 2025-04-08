#ifndef DHT22SENSOR_H // Corrected guard name
#define DHT22SENSOR_H

#include <Arduino.h>
#include <DHT.h> // Using the DHT sensor library by Adafruit

/**
 * @file DHT22Sensor.h
 * @brief Wrapper class for the DHT22 temperature and humidity sensor.
 *
 * Provides a simple interface to initialize and read temperature and humidity
 * values from a DHT22 sensor connected to a specific GPIO pin.
 */
class DHT22Sensor {
public:
    /**
     * @brief Constructor for the DHT22 sensor wrapper.
     * @param pin The GPIO pin number where the DHT22 data pin is connected.
     */
    DHT22Sensor(int pin);

    /**
     * @brief Initializes the DHT sensor library.
     * Call this once in your setup.
     */
    void begin();

    /**
     * @brief Reads the temperature from the sensor.
     * @return The temperature in degrees Celsius (°C). Returns NAN (Not a Number) if the read failed.
     */
    float readTemperature();

    /**
     * @brief Reads the relative humidity from the sensor.
     * @return The relative humidity as a percentage (%). Returns NAN (Not a Number) if the read failed.
     */
    float readHumidity();

private:
    int dhtPin; ///< GPIO pin number for the sensor's data line.
    DHT dht;    ///< Instance of the underlying DHT library object.
};

#endif // DHT22SENSOR_H