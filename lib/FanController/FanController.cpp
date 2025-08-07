// lib/FanController/FanController.cpp
#include "FanController.h"

/**
 * @brief Constructor implementation.
 * @param relayPin The GPIO pin for the fan relay.
 * @param normallyOpenRelay True if HIGH activates relay, false if LOW activates.
 */
FanController::FanController(int relayPin, bool normallyOpenRelay) :
    _relayPin(relayPin),
    _isFanOn(false), // Assume fan is initially off
    _normallyOpenRelay(normallyOpenRelay) {

    if (_normallyOpenRelay) {
        _turnOnSignal = HIGH;
        _turnOffSignal = LOW;
    } else {
        _turnOnSignal = LOW;
        _turnOffSignal = HIGH;
    }
}

/**
 * @brief Initializes the relay pin and sets the fan to an initial off state.
 */
void FanController::begin() {
    pinMode(_relayPin, OUTPUT);
    digitalWrite(_relayPin, _turnOffSignal); // Ensure fan is off initially
    _isFanOn = false;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[FanCtrl] Initialized on pin %d. Fan set to OFF. Relay logic: %s (ON signal: %s).\n",
                      _relayPin,
                      _normallyOpenRelay ? "Normally Open (Active HIGH)" : "Normally Closed (Active LOW)",
                      _turnOnSignal == HIGH ? "HIGH" : "LOW");
    #endif
}

/**
 * @brief Turns the fan ON.
 */
void FanController::turnOn() {
    if (!_isFanOn) { // Only act if currently off to avoid redundant writes and logs
        digitalWrite(_relayPin, _turnOnSignal);
        _isFanOn = true;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[FanCtrl] Pin %d set to %s. Fan commanded ON.\n", _relayPin, _turnOnSignal == HIGH ? "HIGH" : "LOW");
        #endif
    }
}

/**
 * @brief Turns the fan OFF.
 */
void FanController::turnOff() {
    if (_isFanOn) { // Only act if currently on
        digitalWrite(_relayPin, _turnOffSignal);
        _isFanOn = false;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[FanCtrl] Pin %d set to %s. Fan commanded OFF.\n", _relayPin, _turnOffSignal == HIGH ? "HIGH" : "LOW");
        #endif
    }
}

/**
 * @brief Checks if the fan is currently commanded to be ON.
 * @return True if the fan's last command was ON, false otherwise.
 */
bool FanController::isOn() const {
    return _isFanOn;
}