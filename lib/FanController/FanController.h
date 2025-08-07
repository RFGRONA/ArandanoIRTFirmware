// lib/FanController/FanController.h
#ifndef FANCONTROLLER_H
#define FANCONTROLLER_H

#include <Arduino.h>

/**
 * @class FanController
 * @brief Manages the basic operation of a cooling fan connected via a relay.
 *
 * Encapsulates the logic for initializing the relay pin, turning the fan
 * on/off, and checking its current state. The decision-making logic for
 * when to operate the fan is handled externally (e.g., in main.cpp).
 */
class FanController {
public:
    /**
     * @brief Constructor for the FanController class.
     * @param relayPin The GPIO pin number connected to the relay that controls the fan.
     * @param normallyOpenRelay Set to true if the relay is normally open (HIGH signal activates),
     * false if normally closed (LOW signal activates). Default is true.
     */
    FanController(int relayPin, bool normallyOpenRelay = false);

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
     * @return True if the fan is ON (based on last command), false otherwise.
     */
    bool isOn() const;

private:
    int _relayPin;              ///< GPIO pin connected to the fan relay.
    bool _isFanOn;              ///< Tracks the last commanded state of the fan.
    bool _normallyOpenRelay;    ///< Stores the relay logic.
    uint8_t _turnOnSignal;      ///< Signal (HIGH or LOW) to turn the fan ON.
    uint8_t _turnOffSignal;     ///< Signal (HIGH or LOW) to turn the fan OFF.
};

#endif // FANCONTROLLER_H