#include "DHT22Sensor.h"

DHT22Sensor::DHT22Sensor(int pin) : dhtPin(pin), dht(pin, DHT22){}

void DHT22Sensor::begin() {
    dht.begin();
}

float DHT22Sensor::readTemperature() {
    float t = dht.readTemperature();
    if (isnan(t)) {
        return NAN;
    } else {
        return t;
    }
}

float DHT22Sensor::readHumidity() {
    float h = dht.readHumidity();
    if (isnan(h)) {
        return NAN;
    } else {
        return h;
    }
}