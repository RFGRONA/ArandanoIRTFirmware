// lib/BME280Sensor/BME280Sensor.cpp
#include "BME280Sensor.h"

/**
 * @brief Constructor. Inicializa las referencias y la bandera de estado.
 */
BME280Sensor::BME280Sensor(TwoWire &wire) : _bme(), _wire(wire), _isInitialized(false) {}

/**
 * @brief Inicializa el sensor BME280.
 */
bool BME280Sensor::begin(uint8_t i2c_addr) {
    // El bus I2C (Wire.begin) debe ser inicializado antes de llamar a esta función.
    _isInitialized = _bme.begin(i2c_addr, &_wire);
    return _isInitialized;
}

/**
 * @brief Lee la temperatura. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readTemperature() {
    if (!_isInitialized) {
        return NAN; // Not a Number
    }
    return _bme.readTemperature();
}

/**
 * @brief Lee la humedad. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readHumidity() {
    if (!_isInitialized) {
        return NAN;
    }
    return _bme.readHumidity();
}

/**
 * @brief Lee la presión. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readPressure() {
    if (!_isInitialized) {
        return NAN;
    }
    // La librería devuelve la presión en Pascales (Pa). La convertimos a hPa.
    return _bme.readPressure() / 100.0F;
}