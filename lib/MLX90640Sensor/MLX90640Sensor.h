/**
 * @file MLX90640Sensor.h
 * @brief Defines a wrapper class for the MLX90640 Thermal Camera sensor.
 *
 * Provides an interface to initialize and read thermal data frames (32x24 pixels)
 * from an MLX90640 sensor using a specific I2C bus instance and the
 * Adafruit MLX90640 library. Also includes helper methods to calculate
 * average, maximum, and minimum temperatures from the latest frame.
 */
#ifndef MLX90640SENSOR_H
#define MLX90640SENSOR_H

#include <Arduino.h>           // Standard Arduino definitions
#include <Wire.h>              // Required for I2C communication (TwoWire class)
#include <Adafruit_MLX90640.h> // Include the Adafruit MLX90640 library

/**
 * @class MLX90640Sensor
 * @brief Wrapper class simplifying the use of an MLX90640 thermal camera sensor.
 *
 * Encapsulates the Adafruit MLX90640 library object, providing methods to initialize
 * the sensor, read full thermal frames (32x24 = 768 pixels), and access the data
 * either as a raw buffer or through calculated statistics (min, max, average).
 */
class MLX90640Sensor {
public:
    /**
     * @brief Constructor for the MLX90640 sensor wrapper.
     * @param wire Reference to the TwoWire instance (e.g., Wire, Wire1) the sensor is connected to.
     */
    MLX90640Sensor(TwoWire &wire);

    /**
     * @brief Initializes the MLX90640 sensor communication and configuration.
     *
     * Attempts to initialize communication using the default I2C address (0x33).
     * Configures the sensor operating parameters:
     * - Mode: Chess pattern readout (`MLX90640_CHESS`)
     * - ADC Resolution: 18-bit (`MLX90640_ADC_18BIT`)
     * - Refresh Rate: 0.5 Hz (`MLX90640_0_5_HZ`) for lower noise.
     * This method should be called once in `setup()`.
     * @note IMPORTANT: A delay MUST be added in your sketch *after* calling `begin()`
     * and *before* the first call to `readFrame()`. The required delay depends
     * on the refresh rate (e.g., > 1 second for 0.5 Hz, > 0.5s for 1Hz).
     * This allows the sensor time to perform its initial measurements.
     * @return `true` if initialization and configuration were successful, `false` otherwise.
     */
    bool begin();

    /**
     * @brief Reads a new thermal data frame from the sensor into the internal buffer.
     *
     * Calls the underlying library function (`getFrame`) to retrieve all 768 pixel
     * temperature values and store them in the internal `frame` buffer.
     * @return `true` if a complete frame was successfully read, `false` if an error occurred
     * (e.g., I2C communication failure, incomplete frame).
     */
    bool readFrame();

    /**
     * @brief Gets a direct pointer to the internal thermal data buffer.
     *
     * Provides access to the raw temperature data from the last successful `readFrame()` call.
     * The buffer is a flat array containing 768 (`32 * 24`) float values representing
     * pixel temperatures.
     * @return Pointer to the internal float array `frame` containing temperatures in degrees Celsius ($^{\circ}C$).
     * @note The data pointed to is only valid after `readFrame()` returns `true`.
     * Calling this before a successful read or after a failed read will return a pointer
     * to potentially uninitialized or outdated data. The pointer itself remains valid.
     */
    float* getThermalData();

private:
    Adafruit_MLX90640 mlx;    ///< @brief Instance of the underlying Adafruit MLX90640 library object.
    float frame[32 * 24];     ///< @brief Internal buffer to store 768 pixel temperatures (degrees Celsius, $^{\circ}C$) from the last frame read.
    TwoWire &_wire;           ///< @brief Reference to the I2C bus instance (e.g., Wire or Wire1) to be used.
};

#endif // MLX90640SENSOR_H