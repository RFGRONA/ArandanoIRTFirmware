/**
 * @file FanController.h
 * @brief Defines a class to control a cooling fan system via a relay.
 *
 * This class manages the fan's on/off state based on temperature thresholds
 * and provides methods to interact with the fan relay.
 */
#ifndef FANCONTROLLER_H
#define FANCONTROLLER_H

#include <Arduino.h>

// Defines the temperature thresholds for fan control.
// These values ​​can be adjusted according to the system's needs.
const float INTERNAL_HIGH_TEMP_ON = 30.0f; ///< Temperature (C) above which the fans turn on.
const float INTERNAL_LOW_TEMP_OFF = 25.0f; ///< Temperature (C) below which the fans turn off (hysteresis).

/**
 * @class FanController
 * @brief Manages the operation of a cooling fan connected via a relay.
 *
 * Encapsulates the logic for initializing the relay pin, turning the fan
 * on/off, checking its current state, and automatically controlling the fan
 * based on provided temperature readings and predefined thresholds.
 */
class FanController {
public:
    /**
     * @brief Constructor for the FanController class.
     * @param relayPin The GPIO pin number connected to the relay that controls the fan.
     * @param normallyOpenRelay Set to true if the relay is normally open (HIGH signal activates),
     * false if normally closed (LOW signal activates). Default is true.
     */
    FanController(int relayPin, bool normallyOpenRelay = true);

    /**
     * @brief Initializes the fan controller.
     * Sets the relay pin to OUTPUT mode and ensures the fan is initially off.
     * This should be called once in the `setup()` function.
     */
    void begin();

    /**
     * @brief Turns the cooling fan ON.
     * Sends the appropriate signal to the relay pin to activate the fan.
     */
    void turnOn();

    /**
     * @brief Turns the cooling fan OFF.
     * Sends the appropriate signal to the relay pin to deactivate the fan.
     */
    void turnOff();

    /**
     * @brief Checks if the fan is currently considered to be ON.
     * @return True if the fan is ON, false otherwise.
     */
    bool isOn() const;

    /**
     * @brief Controls the fan based on the current internal temperature.
     * Implements hysteresis: turns the fan ON if temperature exceeds TEMP_INTERNA_ALTA_ENCENDER
     * and turns it OFF if temperature drops below TEMP_INTERNA_BAJA_APAGAR.
     * @param currentTemperature The current internal temperature in Celsius.
     */
    void controlTemperature(float currentTemperature);

private:
    int _relayPin;              ///< GPIO pin connected to the fan relay.
    bool _isFanOn;              ///< Tracks the current state of the fan (true if ON, false if OFF).
    bool _normallyOpenRelay;    ///< Stores the relay logic (true for normally open, false for normally closed).
    uint8_t _turnOnSignal;      ///< Signal (HIGH or LOW) to turn the fan ON.
    uint8_t _turnOffSignal;     ///< Signal (HIGH or LOW) to turn the fan OFF.
};

#endif // FANCONTROLLER_H