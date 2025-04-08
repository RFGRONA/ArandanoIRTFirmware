#include "ErrorLogger.h"
#include <ArduinoJson.h> 
#include <HTTPClient.h>  
#include <WiFi.h>
#include "API.h"

API api; 

// Definir un tamaño para el documento JSON. Ajustar si es necesario.
#define JSON_LOG_SIZE 256 

bool ErrorLogger::sendLog(const String& apiUrl, const String& errorSource, const String& errorMessage) {
    
    // NO enviar logs si no hay conexión WiFi o si el backend no está activo
    if (WiFi.status() != WL_CONNECTED || api.checkBackendStatus() == false) {
        return false; 
    }

    // Crear documento JSON
    JsonDocument doc;
    doc["source"] = errorSource;
    doc["message"] = errorMessage;

    // Serializar JSON a String
    String jsonString;
    serializeJson(doc, jsonString);

    // Configurar y enviar petición HTTP POST
    HTTPClient http;
    bool success = false;

    // Usar WiFiClient normal para HTTP. Si tu API fuera HTTPS, necesitarías WiFiClientSecure
    WiFiClient client; 
    http.begin(client, apiUrl); // Usar la versión de begin que toma el cliente
    
    http.addHeader("Content-Type", "application/json");
    
    int httpResponseCode = http.POST(jsonString);

    // Verificar si la respuesta del servidor fue exitosa (códigos 2xx)
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
        success = true;
    } else {
        success = false;
    }

    http.end(); 
    return success;
}