#include "BH1750Sensor.h"

BH1750Sensor::BH1750Sensor(TwoWire &wire, int sda, int scl)
  : _wire(wire), _sda(sda), _scl(scl) {}

bool BH1750Sensor::begin() {
  return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, 0x23, &_wire);
}

float BH1750Sensor::readLightLevel() {
  return lightMeter.readLightLevel();
}