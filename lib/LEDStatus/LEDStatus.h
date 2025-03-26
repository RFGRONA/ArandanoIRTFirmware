#ifndef LEDSTATUS_H
#define LEDSTATUS_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

// Estados definidos para el LED
enum LEDState {
  ERROR_AUTH,    // Error al autenticar
  ERROR_SEND,    // Error al enviar datos
  ERROR_SENSOR,  // Error en algún sensor
  ERROR_DATA,    // Error en los datos
  TAKING_DATA,   // Tomando datos
  SENDING_DATA,  // Enviando datos
  ALL_OK         // Todo ok / en reposo
};

class LEDStatus {
public:
  // Constructor: se utiliza el pin 48 y se asume un único LED WS2812
  LEDStatus();

  // Inicializa el LED WS2812
  void begin();

  // Establece el estado del LED según el enum
  void setState(LEDState state);

  // Apaga el LED
  void turnOffAll();

private:
  // Configuración fija: pin 48 y 1 LED
  Adafruit_NeoPixel pixels;

  // Establece el color del LED
  void setColor(uint8_t r, uint8_t g, uint8_t b);
};

#endif