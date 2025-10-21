// src/EnvironmentTasks.h
#ifndef ENVIRONMENT_TASKS_H
#define ENVIRONMENT_TASKS_H

#include <Arduino.h>
// Include headers for types used in parameters
#include "BH1750Sensor.h"
#include "API.h"
#include "LEDStatus.h"
#include "ConfigManager.h" 
#include "SDManager.h"    
#include "TimeManager.h"
#include "BME280Sensor.h"

// --- Function Prototypes for EnvironmentTasks ---
/** @brief Orchestrates reading environmental sensor data and sending it.
 * @param sdMgr Reference to SDManager for logging and state management.
 * @param timeMgr Reference to TimeManager for time synchronization.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param lightSensor Reference to BH1750 sensor.
 * @param bmeSensor Reference to BME280 sensor.
 * @param sysLed Reference to LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @return True if data read AND sent successfully.
 */
bool performEnvironmentTasks_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, BH1750Sensor& lightSensor, BME280Sensor& bmeSensor, LEDStatus& sysLed, float internalTempForLog);

/** @brief Reads the light sensor value with retries.
 * @param lightSensor Reference to BH1750 sensor.
 * @param lightLevel Output reference for the light level.
 * @return True if read successfully.
 */
bool readLightSensorWithRetry_Env(BH1750Sensor& lightSensor, float &lightLevel);

/** @brief Reads temp, hum, and pressure from the BME sensor with retries.
 * @param bmeSensor Reference to BME280 sensor.
 * @param temperature Output reference for temperature.
 * @param humidity Output reference for humidity.
 * @param pressure Output reference for pressure. 
 * @return True if read successfully.
 */
bool readBmeSensorWithRetry_Env(BME280Sensor& bmeSensor, float &temperature, float &humidity, float &pressure);

/** @brief Sends the collected environmental data to the configured server endpoint.
 * @param sdMgr Reference to SDManager for logging and state management.
 * @param timeMgr Reference to TimeManager for time synchronization.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param lightLevel The light level value.
 * @param temperature The temperature value.
 * @param humidity The humidity value.
 * @param pressure The pressure value.
 * @param sysLed Reference to LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @return True if data sent successfully.
 */
bool sendEnvironmentDataToServer_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, const String& timestamp, float lightLevel, float temperature, float humidity, float pressure,LEDStatus& sysLed, float internalTempForLog); 


#endif // ENVIRONMENT_TASKS_H