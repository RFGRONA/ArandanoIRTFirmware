#include <Adafruit_NeoPixel.h>  // Include the Adafruit NeoPixel library to control WS2812 LEDs

#define PIN 48                  // Define the pin connected to the WS2812 LED (GPIO48)
#define NUMPIXELS 1             // Define the number of NeoPixel LEDs in the strip (in this case, only 1 integrated LED)

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);  // Initialize the NeoPixel library with the number of pixels, pin, and color format

void setup() {
  pixels.begin();            // Initialize the NeoPixel strip.
  pixels.clear();            // Turn off all LEDs initially.
  pixels.show();             // Send the initial state to the LEDs.
  delay(250);                // Brief pause to allow the initial setup to complete.
}

void loop() {
  // Red color
  pixels.setPixelColor(0, pixels.Color(255, 0, 0));  // Set the color of the first LED (index 0) to red.
  pixels.show();            // Send color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Green color
  pixels.setPixelColor(0, pixels.Color(0, 255, 0));  // Set the color of the first LED to green.
  pixels.show();            // Send the updated color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Blue color
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));  // Set the color of the first LED to blue.
  pixels.show();            // Send the updated color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Yellow color
  pixels.setPixelColor(0, pixels.Color(255, 255, 0));  // Set the color of the first LED to yellow (red + green).
  pixels.show();            // Send the updated color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Cyan color
  pixels.setPixelColor(0, pixels.Color(0, 255, 255));  // Set the color of the first LED to cyan (green + blue).
  pixels.show();            // Send the updated color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Magenta color
  pixels.setPixelColor(0, pixels.Color(255, 0, 255));  // Set the color of the first LED to magenta (red + blue).
  pixels.show();            // Send the updated color data to the LEDs.
  delay(1000);              // Wait for one second.

  // Turn off the LED
  pixels.clear();           // Turn off all the LEDs.
  pixels.show();            // Send the update to the LEDs.
  delay(1000);              // Wait for one second.
}