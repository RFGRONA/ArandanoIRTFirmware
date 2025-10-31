#include "ErrorLogger.h"
#include <ArduinoJson.h> // Para construir el payload JSON para la API
#include <HTTPClient.h>  // Para realizar las peticiones POST a la API
#include <WiFi.h>        // Para verificar el estado de la conexión WiFi
#include <math.h>        // Para la comprobación isnan() de la temperatura
#include "SDManager.h"    // Para la escritura local en SD
#include "TimeManager.h"  // Para obtener los timestamps

// Timeout para la petición HTTP de envío de logs (milisegundos)
#define LOG_HTTP_REQUEST_TIMEOUT 5000 

// Definición (instanciación) de las constantes de tipo de log
const char LOG_TYPE_INFO[]    = "INFO";
const char LOG_TYPE_WARNING[] = "WARNING";
const char LOG_TYPE_ERROR[]   = "ERROR";

bool ErrorLogger::sendLog(SDManager& sdManager,
                          TimeManager& timeManager,
                          const String& fullLogUrl, 
                          const String& accessToken, 
                          const char* logType, 
                          const String& logMessage, 
                          float internalTemp) {

    // --- Paso 1: Validación de Parámetros Básicos ---
    if (logType == nullptr || logMessage.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] Skipped logging: Missing logType or message."));
        #endif
        return false; // No se puede registrar sin tipo o mensaje
    }

    // --- Paso 2: Obtener Timestamp ---
    // Obtiene la hora actual o el tiempo de actividad (uptime) si el NTP no se ha sincronizado
    String timestamp = timeManager.getCurrentTimestampString(); 

    // --- Paso 3: Registrar Localmente en la SD (Siempre Intentar) ---
    bool localLogSuccess = false;
    if (sdManager.isSDAvailable()) { 
        // Convierte el 'const char*' (para la API) al 'enum LogLevel' (para la SD)
        LogLevel levelEnum;
        if (strcmp(logType, LOG_TYPE_INFO) == 0) {
            levelEnum = LogLevel::INFO;
        } else if (strcmp(logType, LOG_TYPE_WARNING) == 0) {
            levelEnum = LogLevel::WARNING;
        } else { // Por defecto, ERROR
            levelEnum = LogLevel::ERROR;
        }
        
        // Intenta escribir en el archivo de log de la SD
        localLogSuccess = sdManager.logToFile(timestamp, levelEnum, logMessage, internalTemp);
        #ifdef ENABLE_DEBUG_SERIAL
            if (localLogSuccess) {
                Serial.printf("[ErrorLogger] Log successfully written to SD card. Timestamp: %s\n", timestamp.c_str());
            } else {
                Serial.println(F("[ErrorLogger] Failed to write log to SD card."));
            }
        #endif
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] SD card not available. Cannot write log locally."));
        #endif
    }

    // --- Paso 4: Intentar Enviar Log a API Remota (Condicionalmente) ---
    
    // Solo proceder si hay WiFi y se proporcionó una URL
    if (WiFi.status() == WL_CONNECTED && !fullLogUrl.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLogger] WiFi connected. Attempting to send log to remote API."));
            if (accessToken.isEmpty()) {
                Serial.println(F("[ErrorLogger] Warning: Sending remote log without an access token."));
            }
        #endif

        // Construir el payload JSON para la API
        JsonDocument doc; 
        doc["logType"] = logType; 
        doc["logMessage"] = logMessage;
        // Añadir temperatura interna solo si es un valor válido (no NAN)
        if (!isnan(internalTemp)) { 
            // Serializa como String con 2 decimales
            doc["internalDeviceTemperature"] = serialized(String(internalTemp, 2)); 
        }

        String jsonPayload;
        serializeJson(doc, jsonPayload);

        if (jsonPayload.isEmpty()) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[ErrorLogger] Failed to serialize JSON payload for remote log. Remote send skipped."));
            #endif
            return localLogSuccess; // Retorna solo el éxito local
        }

        // Configurar cliente HTTP
        HTTPClient http;
        http.setReuse(false); // Evitar conexiones "stale" (obsoletas)

        if (http.begin(fullLogUrl)) {
            http.setTimeout(LOG_HTTP_REQUEST_TIMEOUT);
            http.addHeader("Content-Type", "application/json");
            http.addHeader("Connection", "close");
            if (!accessToken.isEmpty()) {
                http.addHeader("Authorization", "Device " + accessToken);
            }

            // Enviar la petición POST
            int httpResponseCode = http.POST(jsonPayload);

            if (httpResponseCode >= 200 && httpResponseCode < 300) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[ErrorLogger] Remote log sent successfully. HTTP Response: %d\n", httpResponseCode);
                #endif
                // (Nota: el éxito remoto no se rastrea en el valor de retorno)
            } else {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[ErrorLogger] Failed to send remote log. HTTP Code: %d\n", httpResponseCode);
                    // (Aquí se maneja la impresión de errores del servidor o del cliente)
                #endif
            }
            http.end(); // Liberar recursos
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[ErrorLogger] HTTP connection failed for remote log URL: %s\n", fullLogUrl.c_str());
            #endif
        }
    } else {
        // (Logs de depuración que explican por qué se omitió el envío remoto)
        #ifdef ENABLE_DEBUG_SERIAL
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println(F("[ErrorLogger] WiFi not connected. Remote log not sent."));
            }
            if (fullLogUrl.isEmpty()) {
                 Serial.println(F("[ErrorLogger] Remote log URL is empty. Remote log not sent."));
            }
        #endif
    }
    
    // El valor de retorno de la función prioriza el éxito del registro local (SD).
    return localLogSuccess; 
}

/**
 * @brief Implementación del registro exclusivo en SD.
 */
bool ErrorLogger::logToSdOnly(SDManager& sdManager,
                              TimeManager& timeManager,
                              LogLevel level,
                              const String& logMessage,
                              float internalTemp) {
                                  
    // --- Paso 1: Validación ---
    if (logMessage.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLoggerSdOnly] Skipped logging: Missing message."));
        #endif
        return false;
    }

    if (!sdManager.isSDAvailable()) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ErrorLoggerSdOnly] SD card not available. Cannot write log."));
        #endif
        return false;
    }

    // --- Paso 2: Obtener Timestamp ---
    String timestamp = timeManager.getCurrentTimestampString();

    // --- Paso 3: Registrar Localmente en la SD ---
    bool localLogSuccess = sdManager.logToFile(timestamp, level, logMessage, internalTemp);
    
    #ifdef ENABLE_DEBUG_SERIAL
        if (localLogSuccess) {
            Serial.printf("[ErrorLoggerSdOnly] Log successfully written to SD card. Timestamp: %s\n", timestamp.c_str());
        } else {
            Serial.println(F("[ErrorLoggerSdOnly] Failed to write log to SD card."));
        }
    #endif

    return localLogSuccess;
}