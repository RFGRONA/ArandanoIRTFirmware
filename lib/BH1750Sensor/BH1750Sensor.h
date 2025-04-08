#ifndef LIGHT_SENSOR_H 
#define LIGHT_SENSOR_H

#include <Wire.h>
#include <BH1750.h> 

/**
 * @file BH1750Sensor.h
 * @brief Wrapper class for the BH1750 light sensor.
 *
 * Provides an interface to initialize and read light levels from a BH1750 sensor
 * using a specific I2C bus instance.
 */
class BH1750Sensor {
  public:
    /**
     * @brief Constructor for the BH1750 sensor wrapper.
     * @param wire Reference to the TwoWire instance (I2C bus) to use.
     * @param sda The SDA pin number (optional, defaults likely handled by TwoWire::begin).
     * @param scl The SCL pin number (optional, defaults likely handled by TwoWire::begin).
     */
    BH1750Sensor(TwoWire &wire, int sda = 47, int scl = 21); // Adjusted defaults to your project

    /**
     * @brief Initializes the BH1750 sensor.
     *
     * Configures the sensor in Continuous High Resolution Mode 2 (1 lx resolution, 120ms measurement time)
     * on the specified I2C address (0x23 default) and bus.
     * Call this once in your setup.
     * @return True if initialization was successful, False otherwise.
     */
    bool begin();

    /**
     * @brief Reads the current light level from the sensor.
     * @return The measured light level in lux (lx). Returns a negative value if the read failed.
     */
    float readLightLevel();

  private:
    BH1750 lightMeter;     ///< Instance of the underlying BH1750 library object.
    int _sda;              ///< SDA pin (primarily for reference, as pins are set in Wire.begin).
    int _scl;              ///< SCL pin (primarily for reference, as pins are set in Wire.begin).
    TwoWire &_wire;        ///< Reference to the I2C bus instance to be used.
};

#endif // LIGHT_SENSOR_H (or BH1750_SENSOR_H)