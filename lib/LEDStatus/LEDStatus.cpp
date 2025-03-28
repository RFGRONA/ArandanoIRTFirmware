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
      // Error al enviar datos: Amarillo 
      setColor(255, 255, 0);
      break;
    case ERROR_SENSOR:
      // Error en algún sensor: Morado 
      setColor(255, 0, 255);
      break;
    case ERROR_DATA:
      // Error en los datos: Cian 
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
      // Sin errores: Blanco 
      setColor(255, 255, 255);
      break;
    case OFF:
      // Apagar el LED
      turnOffAll();
      break;
    case CONNECTING_WIFI:
      // Conectando a WiFi: Naranja 
        setColor(255, 165, 0);
      break;
    case ERROR_WIFI:
      // Error al conectar a WiFi: Rosado
      setColor(255, 105, 180);
      break;
    default:
      turnOffAll();
      break;
  }
}

// Otros colores:
// - Rojo Carmesí / Rojo Oscuro (139, 0, 0 o 180, 0, 0)
// - Ámbar Oscuro / Marrón Rojizo (139, 69, 19) - Puede ser menos visible o confundirse con Naranja si el brillo es bajo.
// - Índigo (75, 0, 130) - Un azul muy oscuro, casi morado, pero diferente del Azul puro y del Morado brillante.
// - Turquesa / Azul Claro (64, 224, 208) - Un azul más claro que el Azul puro, pero no tan brillante como el Verde Lima.
// - Verde Lima / Chartreuse (127, 255, 0) - Un verde más brillante que el Verde puro, pero menos saturado.
// - Se pueden probar colores más claros y oscuros