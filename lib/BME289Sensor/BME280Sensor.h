#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include <Wire.h>

// Declaración anticipada (forward declaration) para evitar incluir
// el pesado header de Adafruit aquí. Solo necesitamos el tipo "puntero a".
class Adafruit_BME280; 

/**
 * @class BME280Sensor
 * @brief Wrapper para el sensor BME280, que simplifica la lectura de
 * temperatura, humedad y presión a través de I2C.
 *
 * @note Esta clase utiliza asignación dinámica de memoria (new/delete)
 * para el objeto de la librería subyacente.
 */
class BME280Sensor {
public:
    /**
     * @brief Constructor para el wrapper del BME280.
     * @param wire Referencia a la instancia de TwoWire (bus I2C) a utilizar.
     */
    BME280Sensor(TwoWire &wire);

    /**
     * @brief Destructor. Libera la memoria del objeto _bme.
     */
    ~BME280Sensor(); 

    /**
     * @brief Inicializa el sensor BME280 en el bus I2C.
     * @param i2c_addr La dirección I2C del sensor. Por defecto es 0x76.
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin(uint8_t i2c_addr = 0x76);

    /**
     * @brief Lee el valor de la temperatura.
     * @return La temperatura en grados Celsius (°C). NAN si no está inicializado.
     */
    float readTemperature();

    /**
     * @brief Lee el valor de la humedad relativa.
     * @return La humedad en porcentaje (%). NAN si no está inicializado.
     */
    float readHumidity();

    /**
     * @brief Lee el valor de la presión barométrica.
     * @return La presión en hectopascales (hPa). NAN si no está inicializado.
     */
    float readPressure();

private:
    ///< Puntero al objeto de la librería Adafruit (gestionado en el heap).
    Adafruit_BME280* _bme;  
    
    ///< Referencia a la instancia del bus I2C (ej. Wire o Wire1).
    TwoWire &_wire;       
    
    ///< Bandera para verificar si begin() fue exitoso.
    bool _isInitialized;  
};

#endif // BME280_SENSOR_H