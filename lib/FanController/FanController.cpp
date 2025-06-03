/**
 * @file FanController.cpp
 * @brief Implements the FanController class methods.
 */
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
        Serial.printf("[FanCtrl] Initialized on pin %d. Fan OFF. Relay logic: %s (ON signal: %s)\n",
                      _relayPin,
                      _normallyOpenRelay ? "Normally Open" : "Normally Closed",
                      _turnOnSignal == HIGH ? "HIGH" : "LOW");
    #endif
}

/**
 * @brief Turns the fan ON.
 */
void FanController::turnOn() {
    digitalWrite(_relayPin, _turnOnSignal);
    _isFanOn = true;
    // #ifdef ENABLE_DEBUG_SERIAL
    //     Serial.printf("[FanCtrl] Pin %d set to %s. Fan turned ON.\n", _relayPin, _turnOnSignal == HIGH ? "HIGH" : "LOW");
    // #endif
}

/**
 * @brief Turns the fan OFF.
 */
void FanController::turnOff() {
    digitalWrite(_relayPin, _turnOffSignal);
    _isFanOn = false;
    // #ifdef ENABLE_DEBUG_SERIAL
    //     Serial.printf("[FanCtrl] Pin %d set to %s. Fan turned OFF.\n", _relayPin, _turnOffSignal == HIGH ? "HIGH" : "LOW");
    // #endif
}

/**
 * @brief Checks if the fan is currently ON.
 * @return True if the fan is ON, false otherwise.
 */
bool FanController::isOn() const {
    return _isFanOn;
}

/**
 * @brief Controls the fan based on current temperature and defined thresholds.
 * Implements hysteresis to prevent rapid cycling.
 * @param currentTemperature The current internal temperature.
 */
void FanController::controlTemperature(float currentTemperature) {
    if (isnan(currentTemperature)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[FanCtrl] Invalid temperature (NAN) received. No action taken.");
        #endif
        return; // Do nothing if temperature reading is invalid
    }

    if (!_isFanOn && currentTemperature > INTERNAL_HIGH_TEMP_ON) {
        turnOn();
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[FanCtrl] Temp (%.1fC) > Threshold_ON (%.1fC). Turning FAN ON.\n", currentTemperature, INTERNAL_HIGH_TEMP_ON);
        #endif
    } else if (_isFanOn && currentTemperature < INTERNAL_LOW_TEMP_OFF) {
        turnOff();
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[FanCtrl] Temp (%.1fC) < Threshold_OFF (%.1fC). Turning FAN OFF.\n", currentTemperature, INTERNAL_LOW_TEMP_OFF);
        #endif
    }
    // No action if temperature is between thresholds, maintaining current fan state (hysteresis)
}