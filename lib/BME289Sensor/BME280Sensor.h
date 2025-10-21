#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <Adafruit_BME280.h>
#include <Wire.h>

/**
 * @class BME280Sensor
 * @brief Wrapper para el sensor BME280, que simplifica la lectura de
 * temperatura, humedad y presión a través de I2C.
 */
class BME280Sensor {
public:
    /**
     * @brief Constructor para el wrapper del BME280.
     * @param wire Referencia a la instancia de TwoWire (bus I2C) a utilizar.
     */
    BME280Sensor(TwoWire &wire);

    /**
     * @brief Inicializa el sensor BME280 en el bus I2C.
     * @param i2c_addr La dirección I2C del sensor. Por defecto es 0x76.
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin(uint8_t i2c_addr = 0x76);

    /**
     * @brief Lee el valor de la temperatura.
     * @return La temperatura en grados Celsius (°C).
     */
    float readTemperature();

    /**
     * @brief Lee el valor de la humedad relativa.
     * @return La humedad en porcentaje (%).
     */
    float readHumidity();

    /**
     * @brief Lee el valor de la presión barométrica.
     * @return La presión en hectopascales (hPa).
     */
    float readPressure();

private:
    Adafruit_BME280 _bme; ///< Instancia de la librería base de Adafruit.
    TwoWire &_wire;       ///< Referencia al bus I2C.
    bool _isInitialized;  ///< Bandera para saber si el sensor fue inicializado.
};

#endif // BME280_SENSOR_H