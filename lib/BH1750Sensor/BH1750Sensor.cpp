/**
 * @file BH1750Sensor.cpp
 * @brief Implementación de los métodos de la clase BH1750Sensor.
 */
#include "BH1750Sensor.h"

// El constructor utiliza la lista de inicialización para guardar la referencia
// al bus I2C y los pines. El cuerpo está vacío intencionalmente.
BH1750Sensor::BH1750Sensor(TwoWire &wire, int sda, int scl)
  : _wire(wire), _sda(sda), _scl(scl) {
    // La inicialización real del hardware ocurre en el método begin().
}

bool BH1750Sensor::begin() {
  // Inicializa el sensor usando la librería base.
  // Se especifica el modo, la dirección I2C (0x23) y la instancia del bus I2C.
  return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE_2, 0x23, &_wire);
}

float BH1750Sensor::readLightLevel() {
  // Simplemente llama al método de la librería base y retorna su valor.
  // La librería BH1750 maneja internamente los códigos de error (valores negativos).
  return lightMeter.readLightLevel();
}