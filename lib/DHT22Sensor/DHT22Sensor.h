#ifndef DHT22Sensor_h
#define DHT22Sensor_h

#include <Arduino.h>
#include <DHT.h>

class DHT22Sensor {
public:
    DHT22Sensor(int pin);
    void begin();
    float readTemperature();
    float readHumidity();
private:
    int dhtPin;
    DHT dht;
};

#endif