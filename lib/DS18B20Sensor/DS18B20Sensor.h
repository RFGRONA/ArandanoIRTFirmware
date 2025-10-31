#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * @class DS18B20Sensor
 * @brief Wrapper para el sensor de temperatura DS18B20, que simplifica su uso
 * gestionando el bus One-Wire y la librería DallasTemperature.
 */
class DS18B20Sensor {
public:
    /**
     * @brief Constructor para el wrapper del DS18B20.
     * @param pin El pin GPIO (bus One-Wire) al que están conectados los sensores.
     */
    DS18B20Sensor(int pin);

    /**
     * @brief Inicializa la comunicación con los sensores en el bus.
     */
    void begin();

    /**
     * @brief Solicita y lee la temperatura del primer sensor (índice 0) en el bus.
     * @return La temperatura en grados Celsius (°C). 
     * Devuelve el valor `DEVICE_DISCONNECTED_C` (-127) si falla la lectura.
     */
    float readTemperature();

private:
    OneWire _oneWire;           ///< Instancia del bus One-Wire asociada al pin.
    DallasTemperature _sensors; ///< Instancia de la librería DallasTemperature (usa OneWire).
};

#endif // DS18B20_SENSOR_H