#include <Adafruit_NeoPixel.h>

#define PIN 48        // Pin conectado al LED WS2812 (GPIO48)
#define NUMPIXELS 1   // Número de LEDs NeoPixel en la tira (en este caso, solo 1 integrado)

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800); // Inicializa la biblioteca NeoPixel

void setup() {
  pixels.begin();           // Inicializa la tira NeoPixel.
  pixels.clear();           // Apaga todos los LEDs inicialmente.
  pixels.show();            // Envía el estado inicial a los LEDs.
  delay(250);               // Pausa breve.
}

void loop() {

  // Color rojo
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Establece el color del primer LED (índice 0) a rojo.
  pixels.show();          // Envía los datos de color a los LEDs.
  delay(1000);            // Espera un segundo.

  // Color verde
  pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Establece el color del primer LED a verde.
  pixels.show();
  delay(1000);

  // Color azul
  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Establece el color del primer LED a azul.
  pixels.show();
  delay(1000);

  pixels.setPixelColor(0, pixels.Color(255, 255, 0)); // Establece el color del primer LED (índice 0) a rojo.
  pixels.show();          // Envía los datos de color a los LEDs.
  delay(1000);            // Espera un segundo.

  // Color verde
  pixels.setPixelColor(0, pixels.Color(0, 255, 255)); // Establece el color del primer LED a verde.
  pixels.show();
  delay(1000);

  // Color azul
  pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Establece el color del primer LED a azul.
  pixels.show();
  delay(1000);

  // Apagar el LED
  pixels.clear();           // Apaga todos los LEDs.
  pixels.show();
  delay(1000);
}