/**
 * @file LEDStatus.cpp
 * @brief Implements the LEDStatus class for controlling a WS2812 NeoPixel status LED.
 */
#include "LEDStatus.h"

// --- Hardware Configuration ---
#define LED_PIN 48   ///< GPIO pin where the WS2812 NeoPixel data line is connected.
#define NUMPIXELS 1  ///< Number of NeoPixels controlled by this instance (should be 1).
// ----------------------------

/**
 * @brief Constructor implementation.
 * Initializes the Adafruit_NeoPixel object and sets the initial LED state to OFF.
 */
LEDStatus::LEDStatus() : 
    pixels(NUMPIXELS, LED_PIN, NEO_GRB + NEO_KHZ800),
    _currentState(OFF) // Initialize current state to OFF
    {
    // The Adafruit_NeoPixel object is configured via the initializer list above.
}

/**
 * @brief Initializes the NeoPixel library communication.
 * Clears any previous color data and sends the 'off' state to the pixel.
 */
void LEDStatus::begin() {
    pixels.begin();       // Initialize Adafruit_NeoPixel library internals.
    pixels.clear();       // Set pixel data buffer to 'off' (0,0,0).
    pixels.show();        // Transmit the buffer data to the physical pixel.
    _currentState = OFF;  // Ensure internal state matches physical state
    delay(50);            // Short delay after initialization (optional, can be adjusted/removed).
}

/**
 * @brief Turns the LED off.
 * Sets the LED to OFF state and updates the physical LED.
 */
void LEDStatus::turnOffAll() {
    pixels.clear(); // Set pixel data buffer to 'off'.
    pixels.show();  // Transmit the update to the pixel.
    _currentState = OFF;
}

/**
 * @brief Internal helper function to set the color of the LED.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 */
void LEDStatus::setColor(uint8_t r, uint8_t g, uint8_t b) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
}

/**
 * @brief Sets the LED to a specific state and color.
 * Updates the internal current state.
 * @param state The desired LEDState.
 */
void LEDStatus::setState(LEDState state) {
    _currentState = state; // Update the current state

    switch(state) {
        case ERROR_AUTH:      // Authentication error state
            setColor(255, 0, 0);   // Red
            break;
        case ERROR_SEND:      // Data transmission error state
            setColor(255, 165, 0); // Orange 
            break;
        case ERROR_SENSOR:    // Sensor read/init error state
            setColor(255, 0, 255); // Purple/Magenta
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
            pixels.clear();
            pixels.show();
            break;
        case CONNECTING_WIFI: // Attempting WiFi connection state
             setColor(255, 223, 0); // Yellow 
            break;
        case ERROR_WIFI:      // WiFi connection failed state
             setColor(255, 105, 180); // Pink
            break;
        case TEMP_HIGH_FANS_ON:  // Internal temperature high, fans are ON
            setColor(139, 0, 0);   // Dark Red 
            break;
        case TEMP_NORMAL_FANS_OFF: // Internal temperature normal, fans are OFF
            setColor(173, 216, 230); // Light Blue 
            break;
        default:              // Handle any undefined or future states
            pixels.clear();
            pixels.show();
            _currentState = OFF; // Ensure a defined state for default
            break;
    }
}

/**
 * @brief Retrieves the current logical state of the LED.
 * @return The current state from the `LEDState` enum.
 */
LEDState LEDStatus::getCurrentState() const { 
    return _currentState;
}


// Other colors:
// - Crimson Red / Dark Red (139, 0, 0 or 180, 0, 0)
// - Dark Amber / Reddish Brown (139, 69, 19) - May be less visible or mistaken for Orange if the brightness is low.
// - Indigo (75, 0, 130) - A very dark blue, almost purple, but different from Pure Blue and Bright Purple.
// - Turquoise / Light Blue (64, 224, 208) - A lighter blue than Pure Blue, but not as bright as Lime Green.
// - Lime Green / Chartreuse (127, 255, 0) - A brighter green than Pure Green, but less saturated.
// - Dark Orange (255, 140, 0) - A darker orange than the default Orange (255, 165, 0).
// - Lighter and darker colors can be tested.