// src/EnvironmentTasks.h
#ifndef ENVIRONMENT_TASKS_H
#define ENVIRONMENT_TASKS_H

#include <Arduino.h>
// Include headers for types used in parameters
#include "BH1750Sensor.h"
#include "DHT22Sensor.h"
#include "API.h"
#include "LEDStatus.h"
#include "ConfigManager.h" 
#include "SDManager.h"    
#include "TimeManager.h"

// --- Function Prototypes for EnvironmentTasks ---
/** @brief Orchestrates reading environmental sensor data and sending it.
 * @param sdMgr Reference to SDManager for logging and state management.
 * @param timeMgr Reference to TimeManager for time synchronization.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param lightSensor Reference to BH1750 sensor.
 * @param dhtSensor Reference to DHT22 sensor.
 * @param sysLed Reference to LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return True if data read AND sent successfully.
 */
bool performEnvironmentTasks_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, BH1750Sensor& lightSensor, DHT22Sensor& dhtSensor, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog);

/** @brief Reads the light sensor value with retries.
 * @param lightSensor Reference to BH1750 sensor.
 * @param lightLevel Output reference for the light level.
 * @return True if read successfully.
 */
bool readLightSensorWithRetry_Env(BH1750Sensor& lightSensor, float &lightLevel);

/** @brief Reads temperature and humidity from the DHT sensor with retries.
 * @param dhtSensor Reference to DHT22 sensor.
 * @param temperature Output reference for temperature.
 * @param humidity Output reference for humidity.
 * @return True if read successfully.
 */
bool readDHTSensorWithRetry_Env(DHT22Sensor& dhtSensor, float &temperature, float &humidity);

/** @brief Sends the collected environmental data to the configured server endpoint.
 * @param sdMgr Reference to SDManager for logging and state management.
 * @param timeMgr Reference to TimeManager for time synchronization.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param lightLevel The light level value.
 * @param temperature The temperature value.
 * @param humidity The humidity value.
 * @param sysLed Reference to LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return True if data sent successfully.
 */
bool sendEnvironmentDataToServer_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, float lightLevel, float temperature, float humidity, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog); 


#endif // ENVIRONMENT_TASKS_H