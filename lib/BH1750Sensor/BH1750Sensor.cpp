#include "BH1750Sensor.h"

// Constructor implementation - Initializes member variables.
// The actual I2C pin assignment happens in the main sketch via Wire.begin().
BH1750Sensor::BH1750Sensor(TwoWire &wire, int sda, int scl)
  : _wire(wire), _sda(sda), _scl(scl) {
  // The BH1750 object 'lightMeter' is implicitly constructed here before the body runs.
}

// Initializes the sensor using the underlying library's begin method.
// Passes the specific I2C bus instance (_wire) to the library.
bool BH1750Sensor::begin() {
  // Attempts to initialize the light meter in Continuous High Res Mode 2,
  // with the default address 0x23, using the provided TwoWire instance.
  return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, 0x23, &_wire);
}

// Reads the light level by calling the underlying library's method.
// Returns the value directly. Error conditions (-1, -2) are handled by the library.
float BH1750Sensor::readLightLevel() {
  return lightMeter.readLightLevel();
}