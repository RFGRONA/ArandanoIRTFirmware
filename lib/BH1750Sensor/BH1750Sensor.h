#ifndef LIGHT_SENSOR_H
#define LIGHT_SENSOR_H

#include <Wire.h>
#include <BH1750.h>

class BH1750Sensor {
  public:
    BH1750Sensor(TwoWire &wire, int sda = 42, int scl = 41);
    bool begin();
    float readLightLevel(); 
  private:
    BH1750 lightMeter;
    int _sda;
    int _scl;
    TwoWire &_wire;
};

#endif
