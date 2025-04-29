/**
 * @file LEDStatus.cpp
 * @brief Implements the LEDStatus class for controlling a WS2812 NeoPixel status LED.
 */
#include "LEDStatus.h"

// --- Hardware Configuration ---
#define LED_PIN 48   ///< GPIO pin where the WS2812 NeoPixel data line is connected.
#define NUMPIXELS 1  ///< Number of NeoPixels controlled by this instance (should be 1).
// ----------------------------

// Constructor: Initializes the Adafruit_NeoPixel object 'pixels' using the defined pin,
// number of pixels, and standard pixel type/speed settings (GRB color order, 800 KHz).
LEDStatus::LEDStatus() : pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800) {
    // The Adafruit_NeoPixel object is configured via the initializer list above.
}

// Initializes the NeoPixel library communication, clears any previous color data
// in the buffer, sends the 'off' state to the pixel, and includes a small delay
// to allow the hardware to stabilize if needed.
void LEDStatus::begin() {
    pixels.begin();       // Initialize Adafruit_NeoPixel library internals.
    pixels.clear();       // Set pixel data buffer to 'off' (0,0,0).
    pixels.show();        // Transmit the buffer data to the physical pixel.
    delay(50);            // Short delay after initialization (optional, can be adjusted/removed).
}

// Turns the LED off by clearing the pixel data buffer and transmitting the update.
void LEDStatus::turnOffAll() {
    pixels.clear(); // Set pixel data buffer to 'off'.
    pixels.show();  // Transmit the update to the pixel.
}

// Internal helper function to set the color of the single pixel (index 0)
// in the buffer and then transmit the update to the physical pixel.
void LEDStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
    // Set the color for the first pixel (index 0) in the buffer.
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    // Transmit the updated buffer data to the pixel.
    pixels.show();
}

// Maps a logical state from the LEDState enum to a specific RGB color
// and updates the physical LED accordingly by calling setColor or turnOffAll.
void LEDStatus::setState(LEDState state) {
    switch(state) {
        case ERROR_AUTH:      // Authentication error state
            setColor(255, 0, 0);   // Red
            break;
        case ERROR_SEND:      // Data transmission error state
            setColor(255, 255, 0); // Yellow
            break;
        case ERROR_SENSOR:    // Sensor read/init error state
            setColor(255, 0, 255); // Purple 
            break;
        case ERROR_DATA:      // Data processing/capture error state
            setColor(0, 255, 255); // Cyan
            break;
        case TAKING_DATA:     // Actively taking measurements/capture state
            setColor(0, 0, 255);   // Blue
            break;
        case SENDING_DATA:    // Actively sending data state
            setColor(0, 255, 0);   // Green
            break;
        case ALL_OK:          // Idle / OK state
            setColor(255, 255, 255); // White 
            break;
        case OFF:             // Explicitly off state
            turnOffAll();
            break;
        case CONNECTING_WIFI: // Attempting WiFi connection state
             setColor(255, 165, 0); // Orange
            break;
        case ERROR_WIFI:      // WiFi connection failed state
             setColor(255, 105, 180); // Pink
            break;
        default:              // Handle any undefined or future states
            turnOffAll();       // Default to off for safety/clarity.
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