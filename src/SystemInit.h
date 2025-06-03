// src/SystemInit.h
#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <Arduino.h>
// Include headers for types used in parameters for initializeSensors_Sys
#include "DHT22Sensor.h"    
#include "BH1750Sensor.h"   
#include "MLX90640Sensor.h" 
#include "OV2640Sensor.h"   

// --- Function Prototypes for SystemInit ---
/** @brief Initializes the Serial port for debugging if ENABLE_DEBUG_SERIAL is defined. */
void initSerial_Sys();

/** @brief Initializes I2C communication.
 * @param sdaPin The I2C SDA pin.
 * @param sclPin The I2C SCL pin.
 * @param frequency The I2C clock frequency.
 */
void initI2C_Sys(int sdaPin, int sclPin, uint32_t frequency = 100000);


/** @brief Initializes all hardware sensors sequentially.
 * @param dht The DHT22 sensor object.
 * @param light The BH1750 light sensor object (uses I2C initialized by initI2C_Sys).
 * @param thermal The MLX90640 thermal sensor object (uses I2C).
 * @param visCamera The OV2640 visual camera object.
 * @return True if all sensors initialized successfully.
 */
bool initializeSensors_Sys(DHT22Sensor& dht, BH1750Sensor& light, MLX90640Sensor& thermal, OV2640Sensor& visCamera);

/** @brief Handles actions to take if sensor initialization fails.
 * This function will not return as it calls deepSleep.
 * It's expected that the calling code (e.g., main.cpp setup) sets the LED error state.
 * @param sleepSeconds_cfg The sleep duration from the global config.
 */
void handleSensorInitFailure_Sys(int sleepSeconds_cfg);

#endif // SYSTEM_INIT_H