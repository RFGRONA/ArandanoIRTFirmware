/**
 * @file BH1750Sensor.cpp
 * @brief Implements the BH1750Sensor wrapper class methods.
 */
#include "BH1750Sensor.h"

// Constructor implementation - Initializes member variables.
// Stores the reference to the I2C bus and the pin numbers for reference.
// The BH1750 object 'lightMeter' is implicitly default-constructed here.
BH1750Sensor::BH1750Sensor(TwoWire &wire, int sda, int scl)
  : _wire(wire), _sda(sda), _scl(scl) {
    // Nothing more needed in the constructor body itself.
    // Initialization happens in the begin() method.
}

// Initializes the sensor using the underlying library's begin method.
// Passes the specific I2C bus instance (_wire) to the library.
bool BH1750Sensor::begin() {
  // Attempts to initialize the light meter in Continuous High Res Mode 2,
  // with the default I2C address 0x23, using the provided TwoWire instance.
  // The return value indicates success (true) or failure (false).
  return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, 0x23, &_wire);
}

// Reads the light level by calling the underlying library's method.
// Returns the value directly. Error conditions (e.g., negative values like -1 or -2)
// indicating read failure are handled by the BH1750 library itself.
float BH1750Sensor::readLightLevel() {
  return lightMeter.readLightLevel();
}