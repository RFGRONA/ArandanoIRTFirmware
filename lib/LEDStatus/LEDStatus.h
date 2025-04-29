/**
 * @file LEDStatus.h
 * @brief Manages a single WS2812 NeoPixel LED for status indication.
 *
 * Provides methods to initialize the LED and set its color based on
 * predefined operational states. Assumes a single NeoPixel connected
 * to a specific pin. Uses the Adafruit NeoPixel library.
 */
#ifndef LEDSTATUS_H
#define LEDSTATUS_H

#include <Adafruit_NeoPixel.h> // Required for NeoPixel control
#include <Arduino.h>           // Required for basic types like uint8_t

/**
 * @brief Defines the possible operational states the system can indicate via the LED color.
 *
 * Each state corresponds to a specific color defined in the implementation (.cpp file).
 */
enum LEDState {
    ALL_OK,          ///< State: Everything OK / Idle. Color: White.
    OFF,             ///< State: LED explicitly turned off. Color: Off.
    ERROR_AUTH,      ///< State: Authentication error occurred. Color: Red.
    ERROR_SEND,      ///< State: Error sending data via HTTP. Color: Yellow.
    ERROR_SENSOR,    ///< State: Error initializing or reading a sensor. Color: Purple.
    ERROR_DATA,      ///< State: Error processing or capturing data (e.g., image). Color: Cyan.
    TAKING_DATA,     ///< State: Currently performing measurements or capture. Color: Blue.
    SENDING_DATA,    ///< State: Currently sending data via HTTP. Color: Green.
    CONNECTING_WIFI, ///< State: Attempting to connect to WiFi network. Color: Orange.
    ERROR_WIFI       ///< State: Failed to connect to WiFi network. Color: Pink.
};

/**
 * @class LEDStatus
 * @brief Manages a single WS2812 NeoPixel for displaying system status via colors.
 *
 * This class encapsulates the Adafruit_NeoPixel library for controlling one pixel.
 * It maps logical system states (defined in `LEDState`) to specific colors.
 */
class LEDStatus {
public:
    /**
     * @brief Constructor for LEDStatus.
     * Initializes the NeoPixel library instance with parameters defined in the .cpp file
     * (pin number, number of pixels, pixel type).
     */
    LEDStatus();

    /**
     * @brief Initializes the NeoPixel LED hardware communication.
     * Sets up the pixel library and ensures the LED is turned off initially.
     * This method should be called once in the `setup()` function of the sketch.
     */
    void begin();

    /**
     * @brief Sets the LED color based on the specified system state.
     * Looks up the color associated with the given state and updates the LED.
     * @param state The desired state from the `LEDState` enum.
     */
    void setState(LEDState state);

    /**
     * @brief Turns the NeoPixel LED completely off.
     * Equivalent to calling `setState(OFF)`.
     */
    void turnOffAll();

private:
    Adafruit_NeoPixel pixels; ///< @brief Instance of the Adafruit NeoPixel library.

    /**
     * @brief Sets the raw RGB color of the NeoPixel LED.
     * This is a low-level helper function used by `setState`.
     * @param r Red component (0-255).
     * @param g Green component (0-255).
     * @param b Blue component (0-255).
     */
    void setColor(uint8_t r, uint8_t g, uint8_t b);
};

#endif // LEDSTATUS_H