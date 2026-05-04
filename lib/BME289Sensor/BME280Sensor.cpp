#include "BME280Sensor.h"
#include <Adafruit_BME280.h> // Se incluye la librería completa en el .cpp

/**
 * @brief Constructor. Asigna memoria para _bme y guarda la referencia a TwoWire.
 */
BME280Sensor::BME280Sensor(TwoWire &wire) : _wire(wire), _isInitialized(false) {
    // Creamos el objeto en el heap (memoria dinámica)
    _bme = new Adafruit_BME280(); 
}

/**
 * @brief Destructor. Libera la memoria asignada a _bme.
 */
BME280Sensor::~BME280Sensor() {
    delete _bme; // Liberamos la memoria para evitar fugas (memory leaks)
}

/**
 * @brief Inicializa el sensor BME280.
 */
bool BME280Sensor::begin(uint8_t i2c_addr) {
    // Precondición: El bus I2C (Wire.begin) debe ser inicializado 
    // por el código principal antes de llamar a esta función.
    
    // Comprobación de seguridad por si falló la asignación de memoria (new)
    if (!_bme) { 
        _isInitialized = false;
        return false;
    }
    
    _isInitialized = _bme->begin(i2c_addr, &_wire);
    return _isInitialized;
}

/**
 * @brief Lee la temperatura. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readTemperature() {
    if (!_isInitialized) {
        return NAN; // Not a Number
    }
    return _bme->readTemperature();
}

/**
 * @brief Lee la humedad. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readHumidity() {
    if (!_isInitialized) {
        return NAN;
    }
    return _bme->readHumidity();
}

/**
 * @brief Lee la presión. Devuelve NAN si el sensor no fue inicializado.
 */
float BME280Sensor::readPressure() {
    if (!_isInitialized) {
        return NAN;
    }
    // La librería devuelve la presión en Pascales (Pa). La convertimos a hPa.
    return _bme->readPressure() / 100.0F;
}