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
 * @brief Reads temperature, humidity, and pressure from the BME280 sensor, with retries.
 * @param bmeSensor Reference to the BME280Sensor object.
 * @param[out] temperature Reference for storing temperature in Celsius.
 * @param[out] humidity Reference for storing relative humidity in %.
 * @param[out] pressure Reference for storing pressure in hPa.
 * @return `true` if all values were read successfully, `false` otherwise.
 */
bool readBmeSensorWithRetry_Env(BME280Sensor& bmeSensor, float &temperature, float &humidity, float &pressure) {
    temperature = NAN;
    humidity = NAN;
    pressure = NAN;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[EnvTasks] Reading environment sensor (BME280)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        temperature = bmeSensor.readTemperature();
        humidity = bmeSensor.readHumidity();
        pressure = bmeSensor.readPressure();

        if (!isnan(temperature) && !isnan(humidity) && !isnan(pressure)) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf(" OK (Temp: %.2f C, Hum: %.1f %%, Pres: %.2f hPa)\n", temperature, humidity, pressure);
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
 * @brief Sends the collected environmental data to the server.
 * Uses EnvironmentDataJSON utility for formatting and sending. Handles token refresh on 401.
 * @param sdMgr Reference to the SDManager for logging and state management.
 * @param timeMgr Reference to the TimeManager for time synchronization.
 * @param cfg Reference to the application's configuration.
 * @param api_obj Reference to the API communication object.
 * @param lightLevel The measured light level.
 * @param temperature The measured temperature (external DHT22).
 * @param humidity The measured humidity (external DHT22).
 * @param sysLed Reference to the LEDStatus object for visual feedback.
 * @return `true` if data was sent successfully, `false` otherwise.
 */
bool sendEnvironmentDataToServer_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, const String& timestamp, float lightLevel, float temperature, float humidity, float pressure, LEDStatus& sysLed, float internalTempForLog) { 
    sysLed.setState(SENDING_DATA);
    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiAmbientDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[EnvTasks] Sending environmental data via HTTP POST...");
        Serial.println("  Target URL: " + fullUrl);
    #endif

    int httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, timestamp, lightLevel, temperature, humidity, pressure);

    if (httpCode == 200 || httpCode == 204) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Environmental data sent successfully."));
        #endif
        return true;
    } else if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Env data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_WARNING, 
                             "Env data send returned 401. Attempting token refresh.", 
                             internalTempForLog); 
        
        int refreshHttpCode = api_obj.performTokenRefresh();
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[EnvTasks] Token refresh successful. Re-trying env data send..."));
            #endif
            ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, 
                                 "Token refreshed successfully after env data 401.", 
                                 internalTempForLog); 
            token = api_obj.getAccessToken();
            httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, timestamp, lightLevel, temperature, humidity, pressure);
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[EnvTasks] Environmental data sent successfully on retry."));
                #endif
                return true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[EnvTasks] Env data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                                     String("Env data send failed on retry after refresh. HTTP: ") + String(httpCode), 
                                     internalTempForLog); 
            }
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[EnvTasks] Token refresh failed after 401. HTTP Code: %d\n", refreshHttpCode);
            #endif
             ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                                  String("Token refresh failed after env data 401. Refresh HTTP: ") + String(refreshHttpCode), 
                                  internalTempForLog); 
        }
    } else { // Other HTTP errors
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[EnvTasks] Error sending environmental data. HTTP Code: %d\n", httpCode);
        #endif
         ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                              String("Failed to send environmental data. HTTP Code: ") + String(httpCode), 
                              internalTempForLog); 
    }

    sysLed.setState(ERROR_SEND);
    delay(1000);
    return false;
}

/**
 * @brief Orchestrates reading all environmental sensors and sending their data.
 */
bool performEnvironmentTasks_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, BH1750Sensor& lightSensor, BME280Sensor& bmeSensor, LEDStatus& sysLed, float internalTempForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] --- Reading Environment Sensors & Sending Data ---"));
    #endif

    String timestamp = timeMgr.getCurrentTimestampString();
    float lightLevel = -1.0f;
    float temperature = NAN;
    float humidity = NAN;
    float pressure = NAN;

    sysLed.setState(TAKING_DATA);

    bool lightOK = readLightSensorWithRetry_Env(lightSensor, lightLevel);
    bool bmeOK = readBmeSensorWithRetry_Env(bmeSensor, temperature, humidity, pressure);

    if (!lightOK || !bmeOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to read one or more environment sensors after retries."));
        #endif
        sysLed.setState(ERROR_SENSOR);
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             String("Failed to read environment sensors."), 
                             internalTempForLog); 
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment sensors read successfully. Preparing to send and archive..."));
    #endif

    // --- Create JSON payload for archival ---
    String envDataJsonString;
    JsonDocument doc;
    doc["timestamp"] = timestamp;
    if (!isnan(lightLevel)) doc["light"] = lightLevel; else doc["light"] = nullptr;
    if (!isnan(temperature)) doc["temperature"] = serialized(String(temperature, 2)); else doc["temperature"] = nullptr;
    if (!isnan(humidity)) doc["humidity"] = serialized(String(humidity, 1)); else doc["humidity"] = nullptr;
    if (!isnan(pressure)) doc["pressure"] = serialized(String(pressure, 2)); else doc["pressure"] = nullptr; 
    
    serializeJson(doc, envDataJsonString);
    // --- End JSON payload creation ---

    bool sentSuccessfully = false;
    if (!envDataJsonString.isEmpty()) {
        sentSuccessfully = sendEnvironmentDataToServer_Env(sdMgr, timeMgr, cfg, api_obj, timestamp, lightLevel, temperature, humidity, pressure, sysLed, internalTempForLog);
        // Archive or save to pending based on send status
        if (sdMgr.isSDAvailable()) {
            String filename = timeMgr.getCurrentTimestampString(true) + "_env.json"; // YYYYMMDD_HHMMSS_env.json
            String targetPath;

            if (sentSuccessfully) {
                targetPath = String(ARCHIVE_ENVIRONMENTAL_DIR) + "/" + filename;
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[EnvTasks] Archiving environmental data to: " + targetPath);
                #endif
            } else {
                targetPath = String(AMBIENT_PENDING_DIR) + "/" + filename;
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[EnvTasks] Saving environmental data to pending: " + targetPath);
                #endif
            }
            
            if (!sdMgr.writeTextFile(targetPath, envDataJsonString)) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[EnvTasks] Failed to write environmental data to SD card at: " + targetPath);
                #endif
                ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, "Failed to write env data to " + targetPath, internalTempForLog);
            }
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[EnvTasks] SD card not available, cannot archive or save pending environmental data.");
            #endif
        }

    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Failed to create JSON string from sensor data. Cannot send or archive."));
        #endif
        // This case implies sensor read was ok, but JSON serialization failed.
        sysLed.setState(ERROR_DATA); 
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Failed to create env JSON for sending/archiving.", 
                             internalTempForLog);
        return false; // Critical failure to form data
    }


    if (!sentSuccessfully) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to send environment data to the server (data saved to pending)."));
        #endif

        return false; 
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment data sent successfully to the server and archived."));
    #endif

    return true;
}