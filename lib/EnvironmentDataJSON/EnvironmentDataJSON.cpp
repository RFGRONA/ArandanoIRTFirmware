#include "EnvironmentDataJSON.h"

bool EnvironmentDataJSON::IOEnvironmentData(
    const String& apiUrl, 
    float lightLevel, 
    float temperature, 
    float humidity
) {
    // Crear documento JSON (se usa StaticJsonDocument para definir la capacidad)
    StaticJsonDocument<256> doc;
    doc["light"] = lightLevel;
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    // Serializar JSON
    String jsonString;
    serializeJson(doc, jsonString);

    // Iniciar cliente HTTP
    HTTPClient http;
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    
    // Enviar solicitud POST
    int httpResponseCode = http.POST(jsonString);
    
    http.end();
    return (httpResponseCode >= 200 && httpResponseCode < 300);
}
