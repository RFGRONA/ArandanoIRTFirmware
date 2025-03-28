#include "MultipartDataSender.h"
#include <ArduinoJson.h>
#include <vector>

bool MultipartDataSender::IOThermalAndImageData(
    const String& apiUrl, 
    float* thermalData, 
    uint8_t* jpegImage, 
    size_t jpegLength
) {
    // Verificar datos de entrada
    if (!thermalData || !jpegImage || jpegLength == 0) {
        return false;
    }

    // Crear documento JSON para datos térmicos
    DynamicJsonDocument doc(4096);  // Suficiente para 32x24 temperaturas
    JsonArray tempArray = doc.createNestedArray("temperatures");
    
    // Copiar datos térmicos al array JSON
    for(int i = 0; i < 32 * 24; i++) {
        tempArray.add(thermalData[i]);
    }

    // Convertir JSON a String
    String thermalJsonString;
    serializeJson(doc, thermalJsonString);

    // Crear límite único para multipart
    String boundary = "----WebKitFormBoundary" + String(esp_random());
    
    // Preparar payload multipart
    std::vector<uint8_t> payload;
    payload.reserve(thermalJsonString.length() + jpegLength + 1024);
    
    // Función auxiliar para agregar texto al payload
    auto appendString = [&payload](const String& str) {
        for (char c : str) {
            payload.push_back(static_cast<uint8_t>(c));
        }
    };

    // Payload para datos térmicos JSON
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"thermal\"\r\n");
    appendString("Content-Type: application/json\r\n\r\n");
    
    // Agregar datos térmicos JSON
    for (char c : thermalJsonString) {
        payload.push_back(static_cast<uint8_t>(c));
    }
    appendString("\r\n");

    // Separador para imagen
    appendString("--" + boundary + "\r\n");
    appendString("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
    appendString("Content-Type: image/jpeg\r\n\r\n");
    
    // Agregar imagen JPEG al payload
    for (size_t i = 0; i < jpegLength; i++) {
        payload.push_back(jpegImage[i]);
    }
    appendString("\r\n");

    // Cerrar payload multipart
    appendString("--" + boundary + "--\r\n");

    // Iniciar cliente HTTP
    HTTPClient http;
    bool connectionSuccess = http.begin(apiUrl);
    if (!connectionSuccess) {
        return false;
    }
    
    // Establecer cabeceras
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http.addHeader("Content-Length", String(payload.size()));

    // Enviar payload con reintentos
    int httpResponseCode = -1;
    int maxRetries = 3;
    for (int retry = 0; retry < maxRetries; retry++) {
        httpResponseCode = http.POST(payload.data(), payload.size());
        
        // Códigos de respuesta exitosos
        if (httpResponseCode >= 200 && httpResponseCode < 300) {
            break;
        }
        
        // Pequeña demora entre reintentos
        delay(500);
    }

    // Finalizar conexión
    http.end();

    // Verificar respuesta final
    return (httpResponseCode >= 200 && httpResponseCode < 300);
}