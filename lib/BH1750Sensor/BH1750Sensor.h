/**
 * @file BH1750Sensor.h
 * @brief Define una clase "wrapper" (envoltorio) para el sensor de luz BH1750.
 *
 * Simplifica la inicialización y lectura de un sensor BH1750
 * sobre una instancia específica de bus I2C (TwoWire).
 */
#ifndef BH1750_SENSOR_H
#define BH1750_SENSOR_H

#include <Wire.h>
#include <BH1750.h> // Librería base del sensor

/**
 * @class BH1750Sensor
 * @brief Clase wrapper que simplifica el uso del sensor de luz BH1750.
 */
class BH1750Sensor {
  public:
    /**
     * @brief Constructor del wrapper para el sensor BH1750.
     * @param wire Referencia a la instancia TwoWire (bus I2C) que utilizará el sensor.
     * @param sda Pin SDA (meramente referencial).
     * @param scl Pin SCL (meramente referencial).
     *
     * @note La configuración real de los pines I2C debe hacerse llamando
     * a `_wire.begin(sda, scl)` antes de llamar a `BH1750Sensor::begin()`.
     */
    BH1750Sensor(TwoWire &wire, int sda = 47, int scl = 21);

    /**
     * @brief Inicializa el sensor BH1750 en el bus I2C especificado.
     *
     * Configura el sensor en Modo Continuo de Alta Resolución 2 (1 lx, ~120ms)
     * en la dirección I2C por defecto (0x23).
     *
     * @return true si la inicialización fue exitosa, false en caso contrario.
     */
    bool begin();

    /**
     * @brief Lee el nivel de luz ambiental actual.
     *
     * @return El nivel de luz medido en lux (lx).
     * Retorna un valor negativo (definido por la librería BH1750, ej: -1 o -2)
     * si la lectura falla.
     */
    float readLightLevel();

  private:
    BH1750 lightMeter;   ///< Instancia de la librería BH1750 subyacente.
    int _sda;            ///< Pin SDA (solo para referencia).
    int _scl;            ///< Pin SCL (solo para referencia).
    TwoWire &_wire;      ///< Referencia al bus I2C (ej: Wire o Wire1) a utilizar.
};

#endif // BH1750_SENSOR_H