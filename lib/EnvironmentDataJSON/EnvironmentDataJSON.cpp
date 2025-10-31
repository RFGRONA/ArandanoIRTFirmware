/**
 * @file EnvironmentDataJSON.cpp
 * @brief Implementa el método estático de la clase EnvironmentDataJSON.
 */
#include "EnvironmentDataJSON.h"
#include <WiFi.h> // Para la comprobación WiFi.status()

// Timeout para las peticiones HTTP de datos ambientales (milisegundos)
#define ENV_DATA_HTTP_REQUEST_TIMEOUT 10000

/**
 * @brief Implementación del método estático.
 */
int EnvironmentDataJSON::IOEnvironmentData(
    const String& fullEnvDataUrl,
    const String& accessToken,
    const String& timestamp,
    float lightLevel,
    float temperature,
    float humidity,
    float pressure
) {
    // --- 1. Validaciones previas (errores locales) ---

    // Error: URL vacía
    if (fullEnvDataUrl.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Skipped sending: Missing fullEnvDataUrl."));
        #endif
        return -1; // Código de error: URL faltante
    }

    // Error: Sin conexión WiFi
    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Skipped sending: No WiFi connection."));
        #endif
        return -2; // Código de error: Sin WiFi
    }

    // --- 2. Construir el payload JSON ---
    JsonDocument doc;
    doc["timestamp"] = timestamp;
    doc["light"] = lightLevel;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["pressure"] = pressure;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    // Error: Fallo al crear el string JSON
    if (jsonPayload.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[EnvDataJSON] Failed to serialize JSON payload."));
        #endif
        return -3; // Código de error: Fallo de serialización JSON
    }

    // --- 3. Configurar y ejecutar la petición HTTP ---
    HTTPClient http;
    int httpResponseCode = -4; // Código de error por defecto (error genérico)

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[EnvDataJSON] Attempting to send env data. URL: %s\n", fullEnvDataUrl.c_str());
        Serial.printf("[EnvDataJSON] Payload: %s\n", jsonPayload.c_str());
    #endif

    // No reutilizar la conexión. Ayuda a evitar problemas de estado ("stale connection")
    // en conexiones HTTP/1.0 o implementaciones simples de ESP32.
    http.setReuse(false);
    
    // http.begin() es para HTTP. Para HTTPS, se debe usar un WiFiClientSecure.
    if (http.begin(fullEnvDataUrl)) { 
        http.setTimeout(ENV_DATA_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Connection", "close"); // Indicar al servidor que cierre la conexión

        // Añadir cabecera de autorización si se proporcionó un token
        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[EnvDataJSON] Warning: Sending environmental data without an access token."));
            #endif
        }

        // Ejecutar la petición POST
        httpResponseCode = http.POST(jsonPayload);

        #ifdef ENABLE_DEBUG_SERIAL
            if (httpResponseCode > 0) {
                Serial.printf("[EnvDataJSON] HTTP Response Code: %d\n", httpResponseCode);
                String responseBody = http.getString();
                if (!responseBody.isEmpty()) Serial.printf("[EnvDataJSON] Response: %s\n", responseBody.c_str());
            } else {
                Serial.printf("[EnvDataJSON] HTTP POST failed, error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            }
        #endif
        
        // Importante: Leer (y descartar) la respuesta del servidor para
        // liberar el buffer del cliente WiFi, incluso si no usamos la respuesta.
        if (httpResponseCode > 0 && http.getSize() > 0) {
             http.getString();
        }

        // Liberar recursos del cliente HTTP
        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[EnvDataJSON] HTTP connection failed for URL: %s\n", fullEnvDataUrl.c_str());
        #endif
        httpResponseCode = -5; // Código de error: http.begin() falló
    }

    return httpResponseCode;
}