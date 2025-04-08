#ifndef MLX90640SENSOR_H // Corrected guard name
#define MLX90640SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h> // Using Adafruit MLX90640 library

/**
 * @file MLX90640Sensor.h
 * @brief Wrapper class for the MLX90640 Thermal Camera sensor.
 *
 * Provides an interface to initialize and read thermal data frames (32x24 pixels)
 * from an MLX90640 sensor using a specific I2C bus instance. Also includes
 * helper methods to calculate average, maximum, and minimum temperatures.
 */
class MLX90640Sensor {
public:
    /**
     * @brief Constructor for the MLX90640 sensor wrapper.
     * @param wire Reference to the TwoWire instance (I2C bus) to use.
     */
    MLX90640Sensor(TwoWire &wire);

    /**
     * @brief Initializes the MLX90640 sensor.
     *
     * Initializes communication using the default I2C address (0x33),
     * configures the sensor mode (Chess pattern), resolution (18-bit ADC),
     * and refresh rate (0.5 Hz).
     * Call this once in your setup.
     * Remember to add a delay (~1.1s or more) after calling begin() before the first readFrame()
     * to allow the sensor to provide the first valid data.
     * @return True if initialization was successful, False otherwise.
     */
    bool begin();

    /**
     * @brief Reads a new thermal data frame from the sensor.
     *
     * Populates the internal `frame` buffer with 768 temperature values.
     * @return True if a frame was successfully read, False otherwise.
     */
    bool readFrame();

    /**
     * @brief Gets a pointer to the internal thermal data buffer.
     *
     * Provides access to the raw temperature data from the last successful `readFrame()` call.
     * The buffer contains 768 (32 * 24) float values.
     * @return Pointer to the internal float array containing temperatures in degrees Celsius (°C).
     * Returns the same pointer on subsequent calls until `readFrame()` is called again.
     * Returns pointer to potentially uninitialized data if `readFrame()` has never succeeded.
     */
    float* getThermalData();

    /**
     * @brief Calculates the average temperature from the last read frame.
     * @return The average temperature in degrees Celsius (°C). Returns calculation based on current buffer content.
     */
    float getAverageTemperature();

    /**
     * @brief Finds the maximum temperature from the last read frame.
     * @return The maximum temperature in degrees Celsius (°C). Returns calculation based on current buffer content.
     */
    float getMaxTemperature();

    /**
     * @brief Finds the minimum temperature from the last read frame.
     * @return The minimum temperature in degrees Celsius (°C). Returns calculation based on current buffer content.
     */
    float getMinTemperature();

private:
    Adafruit_MLX90640 mlx;     ///< Instance of the underlying Adafruit MLX90640 library object.
    float frame[32 * 24];    ///< Internal buffer to store the 768 temperature values (°C) from the last frame read.
    TwoWire &_wire;          ///< Reference to the I2C bus instance to be used.
};

#endif // MLX90640SENSOR_H