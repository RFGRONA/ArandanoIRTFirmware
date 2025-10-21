// lib/DS18B20Sensor/DS18B20Sensor.h
#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include <OneWire.h>
#include <DallasTemperature.h>

/**
 * @class DS18B20Sensor
 * @brief Wrapper para el sensor de temperatura DS18B20, que simplifica su uso.
 */
class DS18B20Sensor {
public:
    /**
     * @brief Constructor para el wrapper del DS18B20.
     * @param pin El pin GPIO al que está conectado el bus One-Wire.
     */
    DS18B20Sensor(int pin);

    /**
     * @brief Inicializa la comunicación con el sensor.
     */
    void begin();

    /**
     * @brief Lee el valor de la temperatura.
     * @return La temperatura en grados Celsius (°C). Devuelve DEVICE_DISCONNECTED_C si falla.
     */
    float readTemperature();

private:
    OneWire _oneWire;           ///< Instancia del bus One-Wire.
    DallasTemperature _sensors; ///< Instancia de la librería DallasTemperature.
};

#endif // DS18B20_SENSOR_H