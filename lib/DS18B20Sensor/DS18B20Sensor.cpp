#include "DS18B20Sensor.h"

/**
 * @brief Constructor.
 * Utiliza la lista de inicialización de C++ para configurar las instancias.
 * _oneWire se inicializa con el pin, y _sensors se inicializa
 * pasándole la dirección de la instancia _oneWire.
 */
DS18B20Sensor::DS18B20Sensor(int pin) : _oneWire(pin), _sensors(&_oneWire) {
    // El cuerpo está vacío intencionalmente; toda la configuración
    // se realiza en la lista de inicialización.
}

/**
 * @brief Inicializa la librería DallasTemperature.
 * (Esta función, a su vez, inicializa el bus One-Wire).
 */
void DS18B20Sensor::begin() {
    _sensors.begin();
}

/**
 * @brief Lee la temperatura del primer sensor encontrado en el bus.
 */
float DS18B20Sensor::readTemperature() {
    // Paso 1: Envía el comando para solicitar las lecturas a TODOS
    // los dispositivos DS18B20 presentes en el bus.
    _sensors.requestTemperatures();
    
    // Paso 2: Obtiene la temperatura del primer sensor (índice 0).
    // Si se produce un error (ej. sensor desconectado), la librería
    // retorna el valor constante DEVICE_DISCONNECTED_C (-127).
    float tempC = _sensors.getTempCByIndex(0);
    
    return tempC;
}