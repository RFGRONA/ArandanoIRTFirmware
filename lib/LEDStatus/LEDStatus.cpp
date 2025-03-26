#include "LEDStatus.h"

#define LED_PIN 48    // Pin de control del LED WS2812
#define NUMPIXELS 1   // Número de LEDs (únicamente 1)

LEDStatus::LEDStatus() : pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800) {
}

void LEDStatus::begin() {
  pixels.begin();
  pixels.clear();
  pixels.show();
  delay(250);
}

void LEDStatus::turnOffAll() {
  pixels.clear();
  pixels.show();
}

void LEDStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

void LEDStatus::setState(LEDState state) {
  switch(state) {
    case ERROR_AUTH:
      // Error al autenticar: Rojo
      setColor(255, 0, 0);
      break;
    case ERROR_SEND:
      // Error al enviar datos: Amarillo (rojo + verde)
      setColor(255, 255, 0);
      break;
    case ERROR_SENSOR:
      // Error en algún sensor: Morado (rojo + azul)
      setColor(255, 0, 255);
      break;
    case ERROR_DATA:
      // Error en los datos: Cian (verde + azul)
      setColor(0, 255, 255);
      break;
    case TAKING_DATA:
      // Tomando datos: Azul
      setColor(0, 0, 255);
      break;
    case SENDING_DATA:
      // Enviando datos: Verde
      setColor(0, 255, 0);
      break;
    case ALL_OK:
      // Todo ok: Blanco (todos los colores)
      setColor(255, 255, 255);
      break;
    default:
      turnOffAll();
      break;
  }
}