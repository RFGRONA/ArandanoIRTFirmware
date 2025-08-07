// src/EnvironmentTasks.cpp
#include "EnvironmentTasks.h"
#include "ErrorLogger.h"         
#include "EnvironmentDataJSON.h" 

// Define SENSOR_READ_RETRIES if it was originally in main.cpp
#define SENSOR_READ_RETRIES 3

/**
 * @brief Reads the BH1750 light sensor value, attempting multiple times if necessary.
 * @param lightSensor Reference to the BH1750Sensor object.
 * @param[out] lightLevel Reference to a float variable where the light level (lux) will be stored.
 * @return `true` if a valid light level was read successfully, `false` otherwise.
 */
bool readLightSensorWithRetry_Env(BH1750Sensor& lightSensor, float &lightLevel) {
    lightLevel = -1.0f; // Initialize to invalid
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[EnvTasks] Reading light sensor (BH1750)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        lightLevel = lightSensor.readLightLevel();
        if (lightLevel >= 0.0f) { // Valid lux readings are typically >= 0
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf(" OK (%.2f lx)\n", lightLevel);
            #endif
            return true;
        }
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(500); // Wait before retrying
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf(" FAILED after %d retries.\n", SENSOR_READ_RETRIES);
    #endif
    return false;
}

/**
 * @brief Reads temperature and humidity from the DHT22 sensor, with retries.
 * @param dhtSensor Reference to the DHT22Sensor object.
 * @param[out] temperature Reference to a float for storing temperature in Celsius.
 * @param[out] humidity Reference to a float for storing relative humidity in %.
 * @return `true` if both temperature and humidity were read successfully, `false` otherwise.
 */
bool readDHTSensorWithRetry_Env(DHT22Sensor& dhtSensor, float &temperature, float &humidity) {
    temperature = NAN; // Initialize to Not-a-Number
    humidity = NAN;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[EnvTasks] Reading temp/humidity sensor (DHT22)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        temperature = dhtSensor.readTemperature();
        delay(100); // Small delay often helps DHT sensors
        humidity = dhtSensor.readHumidity();

        if (!isnan(temperature) && !isnan(humidity)) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf(" OK (Temp: %.2f C, Hum: %.1f %%)\n", temperature, humidity);
            #endif
            return true;
        }
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(1000); // Wait longer for DHT retries
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf(" FAILED after %d retries.\n", SENSOR_READ_RETRIES);
    #endif
    return false;
}

/**
 * @brief Sends the collected environmental data to the server.
 * Uses EnvironmentDataJSON utility for formatting and sending. Handles token refresh on 401.
 * @param cfg Reference to the application's configuration.
 * @param api_obj Reference to the API communication object.
 * @param lightLevel The measured light level.
 * @param temperature The measured temperature (external DHT22).
 * @param humidity The measured humidity (external DHT22).
 * @param sysLed Reference to the LEDStatus object for visual feedback.
 * @param internalTempForLog Internal temperature of the device for logging.
 * @param internalHumForLog Internal humidity of the device for logging.
 * @return `true` if data was sent successfully, `false` otherwise.
 */
bool sendEnvironmentDataToServer_Env(Config& cfg, API& api_obj, float lightLevel, float temperature, float humidity, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog) { 
    sysLed.setState(SENDING_DATA);
    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiAmbientDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[EnvTasks] Sending environmental data via HTTP POST...");
        Serial.println("  Target URL: " + fullUrl);
    #endif

    int httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, lightLevel, temperature, humidity);

    if (httpCode == 200 || httpCode == 204) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Environmental data sent successfully."));
        #endif
        return true;
    } else if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Env data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_WARNING, 
                             "Env data send returned 401. Attempting token refresh.", 
                             internalTempForLog, internalHumForLog); 
        
        int refreshHttpCode = api_obj.performTokenRefresh();
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[EnvTasks] Token refresh successful. Re-trying env data send..."));
            #endif
            ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, 
                                 "Token refreshed successfully after env data 401.", 
                                 internalTempForLog, internalHumForLog); 
            token = api_obj.getAccessToken();
            httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, lightLevel, temperature, humidity);
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[EnvTasks] Environmental data sent successfully on retry."));
                #endif
                return true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[EnvTasks] Env data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, 
                                     "Env data send failed on retry after refresh. HTTP: " + String(httpCode), 
                                     internalTempForLog, internalHumForLog); 
            }
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[EnvTasks] Token refresh failed after 401. HTTP Code: %d\n", refreshHttpCode);
            #endif
             ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, 
                                  "Token refresh failed after env data 401. Refresh HTTP: " + String(refreshHttpCode), 
                                  internalTempForLog, internalHumForLog); 
        }
    } else { // Other HTTP errors or client errors from IOEnvironmentData
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[EnvTasks] Error sending environmental data. HTTP Code: %d\n", httpCode);
        #endif
         ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, 
                              "Failed to send environmental data. HTTP Code: " + String(httpCode), 
                              internalTempForLog, internalHumForLog); 
    }

    sysLed.setState(ERROR_SEND);
    delay(1000);
    return false;
}

/**
 * @brief Orchestrates reading all environmental sensors and sending their data.
 * @param cfg Reference to the application's configuration.
 * @param api_obj Reference to the API communication object.
 * @param lightSensor Reference to the BH1750Sensor object.
 * @param dhtSensor Reference to the DHT22Sensor object.
 * @param sysLed Reference to the LEDStatus object for visual feedback.
 * @param internalTempForLog Internal temperature of the device for logging.
 * @param internalHumForLog Internal humidity of the device for logging.
 * @return `true` if all sensor data was successfully read AND sent, `false` otherwise.
 */
bool performEnvironmentTasks_Env(Config& cfg, API& api_obj, BH1750Sensor& lightSensor, DHT22Sensor& dhtSensor, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] --- Reading Environment Sensors & Sending Data ---"));
    #endif

    float lightLevel = -1.0f;
    float temperature = NAN; // External DHT22 temperature
    float humidity = NAN;    // External DHT22 humidity

    LEDState originalLedState = sysLed.getCurrentState();
    sysLed.setState(TAKING_DATA);

    bool lightOK = readLightSensorWithRetry_Env(lightSensor, lightLevel);
    bool dhtOK = readDHTSensorWithRetry_Env(dhtSensor, temperature, humidity);

    if (!lightOK || !dhtOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to read one or more environment sensors after retries."));
        #endif
        sysLed.setState(ERROR_SENSOR);
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Failed to read environment sensors.", 
                             internalTempForLog, internalHumForLog); 
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment sensors read successfully. Preparing to send..."));
    #endif

    // Pass internalTempForLog and internalHumForLog to sendEnvironmentDataToServer_Env
    if (!sendEnvironmentDataToServer_Env(cfg, api_obj, lightLevel, temperature, humidity, sysLed, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to send environment data to the server."));
        #endif
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment data sent successfully to the server."));
    #endif
    if (sysLed.getCurrentState() != ERROR_SEND && sysLed.getCurrentState() != ERROR_SENSOR) {
         sysLed.setState(originalLedState == TAKING_DATA ? ALL_OK : originalLedState);
    }
    return true;
}