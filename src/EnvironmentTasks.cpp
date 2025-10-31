#include "EnvironmentTasks.h"
#include "ErrorLogger.h"         // Para registrar errores
#include "EnvironmentDataJSON.h" // Para formatear y enviar el JSON

// Define el número de reintentos para la lectura de sensores
#define SENSOR_READ_RETRIES 3

/**
 * @brief Lee el sensor de luz BH1750 con reintentos.
 */
bool readLightSensorWithRetry_Env(BH1750Sensor& lightSensor, float &lightLevel) {
    lightLevel = -1.0f; // Inicializa como inválido
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[EnvTasks] Reading light sensor (BH1750)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        lightLevel = lightSensor.readLightLevel();
        // Una lectura válida de lux debe ser 0.0 o positiva
        if (lightLevel >= 0.0f) { 
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf(" OK (%.2f lx)\n", lightLevel);
            #endif
            return true;
        }
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(500); // Espera antes de reintentar
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf(" FAILED after %d retries.\n", SENSOR_READ_RETRIES);
    #endif
    return false;
}

/**
 * @brief Lee el sensor BME280 (Temp, Hum, Pres) con reintentos.
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

        // Verifica que todas las lecturas sean válidas (no Not-a-Number)
        if (!isnan(temperature) && !isnan(humidity) && !isnan(pressure)) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf(" OK (Temp: %.2f C, Hum: %.1f %%, Pres: %.2f hPa)\n", temperature, humidity, pressure);
            #endif
            return true;
        }
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(500); // Espera antes de reintentar
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf(" FAILED after %d retries.\n", SENSOR_READ_RETRIES);
    #endif
    return false;
}


/**
 * @brief Envía los datos ambientales al servidor, manejando la autenticación (401).
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

    // 1. Primer intento de envío
    int httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, timestamp, lightLevel, temperature, humidity, pressure);

    if (httpCode == 200 || httpCode == 204) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Environmental data sent successfully."));
        #endif
        return true; // Éxito
    
    // 2. Manejo de error de autenticación (401 Unauthorized)
    } else if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Env data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_WARNING, 
                             "Env data send returned 401. Attempting token refresh.", 
                             internalTempForLog); 
        
        // Intenta refrescar el token
        int refreshHttpCode = api_obj.performTokenRefresh();
        
        if (refreshHttpCode == 200) { // Refresco de token exitoso
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[EnvTasks] Token refresh successful. Re-trying env data send..."));
            #endif
            ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, 
                                 "Token refreshed successfully after env data 401.", 
                                 internalTempForLog); 
            
            // 3. Segundo intento de envío (con el nuevo token)
            token = api_obj.getAccessToken();
            httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, timestamp, lightLevel, temperature, humidity, pressure);
            
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[EnvTasks] Environmental data sent successfully on retry."));
                #endif
                return true; // Éxito en el reintento
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[EnvTasks] Env data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                                     String("Env data send failed on retry after refresh. HTTP: ") + String(httpCode), 
                                     internalTempForLog); 
            }
        } else { // Fallo en el refresco del token
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[EnvTasks] Token refresh failed after 401. HTTP Code: %d\n", refreshHttpCode);
            #endif
             ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                                  String("Token refresh failed after env data 401. Refresh HTTP: ") + String(refreshHttpCode), 
                                  internalTempForLog); 
        }
    } else { // Otros errores HTTP (500, 404, timeouts < 0, etc.)
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[EnvTasks] Error sending environmental data. HTTP Code: %d\n", httpCode);
        #endif
         ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, token, LOG_TYPE_ERROR, 
                              String("Failed to send environmental data. HTTP Code: ") + String(httpCode), 
                              internalTempForLog); 
    }

    // Si llegamos aquí, todos los intentos fallaron
    sysLed.setState(ERROR_SEND);
    delay(1000);
    return false;
}

/**
 * @brief Orquesta la lectura, envío y archivo/guardado de datos ambientales.
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

    // --- 1. Leer Sensores ---
    bool lightOK = readLightSensorWithRetry_Env(lightSensor, lightLevel);
    bool bmeOK = readBmeSensorWithRetry_Env(bmeSensor, temperature, humidity, pressure);

    if (!lightOK || !bmeOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to read one or more environment sensors after retries."));
        #endif
        sysLed.setState(ERROR_SENSOR);
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             String("Failed to read environment sensors."), 
                             internalTempForLog); 
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment sensors read successfully. Preparing to send and archive..."));
    #endif

    // --- 2. Crear el payload JSON (para guardar en SD) ---
    // Este JSON se crea *independientemente* del envío, para asegurar
    // que los datos se guarden en la SD (ya sea en 'archive' o 'pending').
    String envDataJsonString;
    JsonDocument doc;
    doc["timestamp"] = timestamp;
    if (!isnan(lightLevel)) doc["light"] = lightLevel; else doc["light"] = nullptr;
    if (!isnan(temperature)) doc["temperature"] = serialized(String(temperature, 2)); else doc["temperature"] = nullptr;
    if (!isnan(humidity)) doc["humidity"] = serialized(String(humidity, 1)); else doc["humidity"] = nullptr;
    if (!isnan(pressure)) doc["pressure"] = serialized(String(pressure, 2)); else doc["pressure"] = nullptr; 
    
    serializeJson(doc, envDataJsonString);
    // --- Fin creación JSON ---

    bool sentSuccessfully = false;
    
    if (envDataJsonString.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Failed to create JSON string from sensor data. Cannot send or archive."));
        #endif
        sysLed.setState(ERROR_DATA); 
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Failed to create env JSON for sending/archiving.", 
                             internalTempForLog);
        return false; // Falla crítica si no se puede formar el JSON
    }

    // --- 3. Intentar Enviar Datos ---
    sentSuccessfully = sendEnvironmentDataToServer_Env(sdMgr, timeMgr, cfg, api_obj, timestamp, lightLevel, temperature, humidity, pressure, sysLed, internalTempForLog);
    
    // --- 4. Guardar en SD (Archive o Pending) ---
    if (sdMgr.isSDAvailable()) {
        String filename = timeMgr.getCurrentTimestampString(true) + "_env.json"; // Formato YYYYMMDD_HHMMSS_env.json
        String targetPath;

        if (sentSuccessfully) {
            // Éxito: Guardar en 'archive'
            targetPath = String(ARCHIVE_ENVIRONMENTAL_DIR) + "/" + filename;
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[EnvTasks] Archiving environmental data to: " + targetPath);
            #endif
        } else {
            // Fallo: Guardar en 'pending' para reintentar luego
            targetPath = String(AMBIENT_PENDING_DIR) + "/" + filename;
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[EnvTasks] Saving environmental data to pending: " + targetPath);
            #endif
        }
        
        // Escribir el archivo JSON en la ruta decidida
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
        // Si no hay SD, el log de "falla de envío" (si ocurrió) ya se intentó
        // enviar a la API, pero el log local falla.
        ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::WARNING, "SD card not available, could not save env data.", internalTempForLog);
    }


    if (!sentSuccessfully) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvTasks] Error: Failed to send environment data to the server (data saved to pending)."));
        #endif
        return false; // Retorna 'false' si el envío falló (aunque se haya guardado en pending)
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[EnvTasks] Environment data sent successfully to the server and archived."));
    #endif
    return true; // Retorna 'true' solo si el envío fue exitoso
}