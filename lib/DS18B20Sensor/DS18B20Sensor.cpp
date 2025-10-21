// lib/DS18B20Sensor/DS18B20Sensor.cpp
#include "DS18B20Sensor.h"

/**
 * @brief Constructor. Configura las instancias de OneWire y DallasTemperature.
 */
DS18B20Sensor::DS18B20Sensor(int pin) : _oneWire(pin), _sensors(&_oneWire) {}

/**
 * @brief Inicializa la librería DallasTemperature.
 */
void DS18B20Sensor::begin() {
    _sensors.begin();
}

/**
 * @brief Lee la temperatura del primer sensor encontrado en el bus.
 */
float DS18B20Sensor::readTemperature() {
    // Envía el comando para solicitar las lecturas a todos los dispositivos en el bus
    _sensors.requestTemperatures();
    
    // Obtenemos la temperatura del primer sensor (índice 0)
    // La librería devuelve DEVICE_DISCONNECTED_C (-127) si hay un error.
    float tempC = _sensors.getTempCByIndex(0);
    
    return tempC;
}