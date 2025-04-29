/**
 * @file BH1750Sensor.h
 * @brief Defines a wrapper class for the BH1750 light sensor.
 *
 * Provides an interface to initialize and read light levels from a BH1750 sensor
 * using a specific I2C bus instance.
 */
#ifndef BH1750_SENSOR_H // Renamed include guard for consistency
#define BH1750_SENSOR_H

#include <Wire.h>
#include <BH1750.h> // Include the library for the sensor

/**
 * @class BH1750Sensor
 * @brief Wrapper class simplifying the use of a BH1750 light sensor.
 *
 * This class encapsulates the BH1750 library object and provides methods
 * to initialize the sensor on a specific I2C bus and read light levels.
 */
class BH1750Sensor {
  public:
    /**
     * @brief Constructor for the BH1750 sensor wrapper.
     * @param wire Reference to the TwoWire instance (I2C bus) the sensor is connected to.
     * @param sda The SDA pin number used by the TwoWire instance. (Default: 47 for your project)
     * @param scl The SCL pin number used by the TwoWire instance. (Default: 21 for your project)
     * @note The actual pin assignment for the I2C bus must be done by calling
     * `_wire.begin(sda, scl)` before calling `BH1750Sensor::begin()`.
     * The sda/scl parameters here are primarily for reference within this class.
     */
    BH1750Sensor(TwoWire &wire, int sda = 47, int scl = 21);

    /**
     * @brief Initializes the BH1750 sensor.
     *
     * Configures the sensor using the BH1750 library. It sets the sensor to
     * Continuous High Resolution Mode 2 (1 lx resolution, ~120ms measurement time)
     * on the specified I2C address (0x23 by default) and uses the I2C bus instance
     * provided in the constructor.
     * @note This method must be called once (e.g., in `setup()`) after the I2C bus
     * (`_wire`) has been initialized with `_wire.begin(sda, scl)`.
     * @return true if initialization was successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Reads the current ambient light level from the sensor.
     *
     * Performs a measurement reading using the BH1750 library.
     * @return The measured light level in lux (lx). Returns a negative value
     * (typically -1 or -2 as defined by the BH1750 library) if the read failed
     * or if the sensor is saturated/under range in certain modes (though less common in Mode 2).
     */
    float readLightLevel();

  private:
    BH1750 lightMeter;   ///< @brief Instance of the underlying BH1750 library object.
    int _sda;            ///< @brief SDA pin number (for reference; actual pin set via TwoWire::begin).
    int _scl;            ///< @brief SCL pin number (for reference; actual pin set via TwoWire::begin).
    TwoWire &_wire;      ///< @brief Reference to the I2C bus instance (e.g., Wire or Wire1) to be used.
};

#endif // BH1750_SENSOR_H