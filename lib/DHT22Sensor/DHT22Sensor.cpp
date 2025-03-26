#include "DHT22Sensor.h"

DHT22Sensor::DHT22Sensor(int pin) : dhtPin(pin), dht(pin, DHT22), errorFlag(false) {}

void DHT22Sensor::begin() {
    dht.begin();
    errorFlag = false;
}

float DHT22Sensor::readTemperature() {
    float t = dht.readTemperature();
    if (isnan(t)) {
        errorFlag = true;
        return NAN;
    } else {
        errorFlag = false;
        return t;
    }
}

float DHT22Sensor::readHumidity() {
    float h = dht.readHumidity();
    if (isnan(h)) {
        errorFlag = true;
        return NAN;
    } else {
        errorFlag = false;
        return h;
    }
}

bool DHT22Sensor::hasError() {
    return errorFlag;
}