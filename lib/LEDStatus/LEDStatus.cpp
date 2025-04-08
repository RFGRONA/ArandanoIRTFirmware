#include "LEDStatus.h"

// --- Configuration ---
#define LED_PIN 48    ///< GPIO pin where the WS2812 NeoPixel data line is connected.
#define NUMPIXELS 1   ///< Number of NeoPixels controlled by this instance.
// ---------------------

// Constructor: Initializes the Adafruit_NeoPixel object using the defined pin,
// number of pixels, and pixel type/speed settings.
LEDStatus::LEDStatus() : pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800) {
  // Initialization list handles the setup
}

// Initializes the NeoPixel library, clears any previous color,
// shows the cleared state (off), and adds a small delay.
void LEDStatus::begin() {
  pixels.begin();      // Initialize NeoPixel library
  pixels.clear();      // Set pixel data to 'off'
  pixels.show();       // Send updated color to the pixel
  delay(250);          // Short delay to ensure initialization completes
}

// Turns the LED off by clearing the pixel data and sending the update.
void LEDStatus::turnOffAll() {
  pixels.clear();
  pixels.show();
}

// Sets the color of the single pixel (index 0) and sends the update.
void LEDStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b)); // Set color data in buffer
  pixels.show();                                   // Send data to the pixel
}

// Sets the LED color based on the predefined states in the LEDState enum.
void LEDStatus::setState(LEDState state) {
  switch(state) {
    case ERROR_AUTH:    // Authentication error: Red
      setColor(255, 0, 0);
      break;
    case ERROR_SEND:    // Data send error: Yellow
      setColor(255, 255, 0);
      break;
    case ERROR_SENSOR:  // Sensor error: Purple
      setColor(255, 0, 255);
      break;
    case ERROR_DATA:    // Data capture/processing error: Cyan
      setColor(0, 255, 255);
      break;
    case TAKING_DATA:   // Taking data: Blue
      setColor(0, 0, 255);
      break;
    case SENDING_DATA:  // Sending data: Green
      setColor(0, 255, 0);
      break;
    case ALL_OK:        // Idle / OK: White
      setColor(255, 255, 255);
      break;
    case OFF:           // Off state
      turnOffAll();
      break;
    case CONNECTING_WIFI: // Connecting to WiFi: Orange
        setColor(255, 165, 0);
      break;
    case ERROR_WIFI:    // WiFi connection error: Pink
      setColor(255, 105, 180);
      break;
    default:            // Unknown state: Turn off
      turnOffAll();
      break;
  }
}


// Other colors:
// - Crimson Red / Dark Red (139, 0, 0 or 180, 0, 0)
// - Dark Amber / Reddish Brown (139, 69, 19) - May be less visible or mistaken for Orange if the brightness is low.
// - Indigo (75, 0, 130) - A very dark blue, almost purple, but different from Pure Blue and Bright Purple.
// - Turquoise / Light Blue (64, 224, 208) - A lighter blue than Pure Blue, but not as bright as Lime Green.
// - Lime Green / Chartreuse (127, 255, 0) - A brighter green than Pure Green, but less saturated.
// - Lighter and darker colors can be tested.