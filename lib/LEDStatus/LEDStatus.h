#ifndef LEDSTATUS_H
#define LEDSTATUS_H

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>

/**
 * @file LEDStatus.h
 * @brief Manages a single WS2812 NeoPixel LED for status indication.
 *
 * Provides methods to initialize the LED and set its color based on
 * predefined operational states. Assumes a single NeoPixel connected
 * to a specific pin.
 */

/**
 * @brief Defines the possible states the system can be in, represented by the LED color.
 */
enum LEDState {
  ALL_OK,         ///< Everything OK / Idle state (White).
  OFF,            ///< LED turned off.
  ERROR_AUTH,     ///< Authentication error (Red).
  ERROR_SEND,     ///< Error sending data via HTTP (Yellow).
  ERROR_SENSOR,   ///< Error initializing or reading a sensor (Purple).
  ERROR_DATA,     ///< Error processing or capturing data (Cyan).
  TAKING_DATA,    ///< Currently taking measurements/capturing images (Blue).
  SENDING_DATA,   ///< Currently sending data via HTTP (Green).
  CONNECTING_WIFI,///< Attempting to connect to WiFi (Orange).
  ERROR_WIFI      ///< Failed to connect to WiFi (Pink).
};

class LEDStatus {
public:
  /**
   * @brief Constructor for LEDStatus.
   * Initializes the NeoPixel library for a single pixel on a predefined pin (see cpp file).
   */
  LEDStatus();

  /**
   * @brief Initializes the NeoPixel LED.
   * Sets up the pixel library and turns the LED off initially. Call once in setup.
   */
  void begin();

  /**
   * @brief Sets the LED color based on the specified system state.
   * @param state The desired state from the LEDState enum.
   */
  void setState(LEDState state);

  /**
   * @brief Turns the NeoPixel LED off.
   */
  void turnOffAll();

private:
  Adafruit_NeoPixel pixels; ///< NeoPixel library instance.

  /**
   * @brief Sets the raw RGB color of the NeoPixel.
   * @param r Red component (0-255).
   * @param g Green component (0-255).
   * @param b Blue component (0-255).
   */
  void setColor(uint8_t r, uint8_t g, uint8_t b);
};

#endif // LEDSTATUS_H