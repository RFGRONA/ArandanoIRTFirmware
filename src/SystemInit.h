// src/SystemInit.h
#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <Arduino.h>
// Include headers for types used in parameters for initializeSensors_Sys
#include "DHT22Sensor.h"    
#include "BH1750Sensor.h"   
#include "MLX90640Sensor.h" 
#include "OV2640Sensor.h"
#include "WiFiManager.h"    
#include "LEDStatus.h"      
#include "ConfigManager.h"  
#include "API.h"            
#include "SDManager.h"      
#include "TimeManager.h"
#include "CycleController.h" 

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
 * This function will not return as it now halts the device.
 * It's expected that the calling code (e.g., main.cpp setup) sets the LED error state.
 */
void handleSensorInitFailure_Sys();

/**
 * @brief Initializes and robustly connects to WiFi with retries.
 * This function is blocking and will halt the device on critical failure. It does not return on failure.
 * @param wifiMgr Reference to the WiFiManager object.
 * @param led Reference to the LEDStatus object.
 * @param cfg Reference to the global Config object.
 * @param api_comm Pointer to the API communication object.
 * @param sdMgr Reference to the SDManager object (for logging connection failures).
 * @param timeMgr Reference to the TimeManager object (for logging connection failures).
 * @param visCamera Reference to OV2640Sensor (unused in new version, but kept for compatibility until main is refactored).
 * @return True if WiFi connected successfully.
 */
bool initializeWiFi_Sys(WiFiManager& wifiMgr, LEDStatus& led, Config& cfg, API* api_comm, 
                        SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera);

/**
 * @brief Initializes and robustly syncs NTP time with retries.
 * This function is blocking and will halt the device on critical failure. It does not return on failure.
 * @param timeMgr Reference to the TimeManager object.
 * @param sdMgr Reference to the SDManager object (for logging sync status).
 * @param api_comm Pointer to the API communication object (for logging sync status).
 * @param cfg Reference to the global Config object (for API paths for logging).
 * @param gmtOffset_sec GMT offset in seconds for the local timezone.
 * @param daylightOffset_sec Daylight saving time offset in seconds.
 * @return True if NTP sync was successful.
 */
bool initializeNTP_Sys(TimeManager& timeMgr, SDManager& sdMgr, API* api_comm, Config& cfg,
                       long gmtOffset_sec, int daylightOffset_sec);

#endif // SYSTEM_INIT_H