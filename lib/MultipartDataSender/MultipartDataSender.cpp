/**
 * @file MultipartDataSender.cpp
 * @brief Implementa los métodos de la clase de utilidad MultipartDataSender.
 */
#include "MultipartDataSender.h"
#include <ArduinoJson.h> 
#include <vector>        // Se usa std::vector para construir dinámicamente el payload
#include <esp_random.h>  // Para generar el 'boundary' aleatorio
#include <math.h>        // Para INFINITY, NAN, isnan
#include <WiFi.h>        // Para la comprobación WiFi.status()

// Timeout para peticiones HTTP que envían datos de captura (milisegundos)
#define CAPTURE_DATA_HTTP_REQUEST_TIMEOUT 20000

// --- Métodos Públicos Estáticos ---

/* static */ int MultipartDataSender::IOThermalAndImageData(
    const String& fullCaptureDataUrl,
    const String& accessToken,
    const String& timestamp,
    float* thermalData,
    uint8_t* jpegImage,
    size_t jpegLength
) {
    // --- Paso 1: Validar Datos de Entrada ---
    if (fullCaptureDataUrl.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Invalid input: Missing fullCaptureDataUrl."));
        #endif
        return -11; // Error cliente: URL faltante
    }
    
    // Los datos térmicos son obligatorios
    if (thermalData == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Invalid input: Null pointer for thermal data."));
        #endif
        return -12; // Error cliente: Datos térmicos nulos
    }
     if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MultipartSender Error] Skipped sending: No WiFi connection."));
        #endif
        return -13; // Error cliente: Sin WiFi
    }
    // NOTA: La imagen (jpegImage) SÍ puede ser nula (opcional).

    // --- Paso 2: Crear Payload JSON para Datos Térmicos ---
    String thermalJsonString = createThermalJson(timestamp, thermalData);
    if (thermalJsonString.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Failed to create thermal JSON."));
        #endif
        return -14; // Error cliente: Fallo al crear JSON
    }

    // --- Paso 3: Generar un String de Límite (Boundary) Único ---
    // Este string separa las diferentes partes del formulario
    String boundary = "----WebKitFormBoundaryESP32-" + String(esp_random(), HEX) + String(esp_random(), HEX);

    // --- Paso 4: Construir el Payload Multipart Completo ---
    // Pasa los datos de la imagen; la función build..() manejará si es nula.
    std::vector<uint8_t> payload = buildMultipartPayload(boundary, thermalJsonString, jpegImage, jpegLength);
    if (payload.empty()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println(F("[MultipartSender Error] Failed to build multipart payload."));
        #endif
        return -15; // Error cliente: Fallo al construir payload
    }

    // --- Paso 5: Realizar la Petición HTTP POST ---
    return performHttpPost(fullCaptureDataUrl, accessToken, boundary, payload);
}

// --- Métodos Privados Estáticos de Ayuda (Helpers) ---

/**
 * @brief Crea un string JSON con estadísticas y datos crudos.
 */
/* static */ String MultipartDataSender::createThermalJson(const String& timestamp, float* thermalData) {
    // 1. Calcular estadísticas
    float maxTemp = calculateMaxTemperature(thermalData);
    float minTemp = calculateMinTemperature(thermalData);
    float avgTemp = calculateAverageTemperature(thermalData);

    // Si los cálculos fallan (ej. todos NaN), abortar.
    if (isinf(maxTemp) || isinf(minTemp) || isnan(avgTemp)) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Warning] Thermal stats calculation resulted in Inf/NaN.");
         #endif
         return "";
    }

    // 2. Construir el documento JSON
    JsonDocument doc;
    doc["timestamp"] = timestamp;
    doc["max_temp"] = maxTemp;
    doc["min_temp"] = minTemp;
    doc["avg_temp"] = avgTemp;

    // 3. Añadir el array de temperaturas
    JsonArray tempArray = doc["temperatures"].to<JsonArray>();
    if (tempArray.isNull()) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to create JSON array for thermal data.");
        #endif
        return ""; // Fallo de alocación de memoria
    }

    for(int i = 0; i < THERMAL_PIXELS; ++i) {
        if (isnan(thermalData[i])) {
             tempArray.add(nullptr); // Representar NaN como 'null' en JSON
        } else {
             tempArray.add(thermalData[i]);
        }
    }

    // 4. Serializar a String
    String jsonString;
    size_t written = serializeJson(doc, jsonString);

    if (written == 0) {
         #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("[MultipartSender Error] Failed to serialize thermal JSON data.");
        #endif
        return "";
    }

    #ifdef ENABLE_DEBUG_SERIAL
       Serial.printf("[MultipartSender] Generated Thermal JSON String Length: %d\n", jsonString.length());
    #endif
    return jsonString;
}

// --- Implementación de cálculos estadísticos ---

float MultipartDataSender::calculateMaxTemperature(float* thermalData) {
    if (thermalData == nullptr) return -INFINITY;
    float maxTemp = -INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        // Ignorar valores NaN
        if (!isnan(thermalData[i]) && thermalData[i] > maxTemp) {
            maxTemp = thermalData[i];
        }
    }
    return maxTemp;
}

float MultipartDataSender::calculateMinTemperature(float* thermalData) {
     if (thermalData == nullptr) return INFINITY;
    float minTemp = INFINITY;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        // Ignorar valores NaN
        if (!isnan(thermalData[i]) && thermalData[i] < minTemp) {
            minTemp = thermalData[i];
        }
    }
    return minTemp;
}

float MultipartDataSender::calculateAverageTemperature(float* thermalData) {
    if (thermalData == nullptr) return NAN;
    double sumTemp = 0.0; // Usar double para precisión en la suma
    int validPixelCount = 0;
    for (int i = 0; i < THERMAL_PIXELS; ++i) {
        // Ignorar valores NaN
        if (!isnan(thermalData[i])) {
            sumTemp += thermalData[i];
            validPixelCount++;
        }
    }
    // Evitar división por cero
    return (validPixelCount > 0) ? (float)(sumTemp / validPixelCount) : NAN;
}


/**
 * @brief Construye el payload multipart/form-data como un vector de bytes.
 */
/* static */ std::vector<uint8_t> MultipartDataSender::buildMultipartPayload(
    const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength
) {
    std::vector<uint8_t> payload;
    
    // Estimar tamaño para reservar memoria (mejora rendimiento)
    size_t estimatedSize = thermalJson.length() + (jpegImage ? jpegLength : 0) + 512; // 512 bytes para cabeceras/boundaries
    payload.reserve(estimatedSize);

    // Funciones 'lambda' para añadir datos al vector de bytes
    auto appendString = [&payload](const String& str) {
        payload.insert(payload.end(), str.c_str(), str.c_str() + str.length());
    };
    auto appendRaw = [&payload](const char* str) {
        payload.insert(payload.end(), str, str + strlen(str));
    };

    // --- Parte 1: Datos Térmicos (JSON) --- (Siempre incluida)
    appendString("--" + boundary + "\r\n");
    appendRaw("Content-Disposition: form-data; name=\"thermal\"\r\n");
    appendRaw("Content-Type: application/json\r\n\r\n");
    appendString(thermalJson);
    appendRaw("\r\n");

    // --- Parte 2: Datos de Imagen (JPEG) --- (Incluida condicionalmente)
    if (jpegImage != nullptr && jpegLength > 0) {
        appendString("--" + boundary + "\r\n");
        appendRaw("Content-Disposition: form-data; name=\"image\"; filename=\"camera.jpg\"\r\n");
        appendRaw("Content-Type: image/jpeg\r\n\r\n");
        // Insertar los bytes crudos de la imagen
        payload.insert(payload.end(), jpegImage, jpegImage + jpegLength);
        appendRaw("\r\n");
    }

    // --- Límite (Boundary) de Cierre ---
    appendString("--" + boundary + "--\r\n");

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MultipartSender] Built multipart payload. Total size: %d bytes.\n", payload.size());
    #endif

    return payload;
}

/**
 * @brief Realiza la petición HTTP POST con el payload multipart.
 */
/* static */ int MultipartDataSender::performHttpPost(
    const String& apiUrl,
    const String& accessToken,
    const String& boundary,
    const std::vector<uint8_t>& payload
) {
    HTTPClient http;
    int httpResponseCode = -16; // Error cliente: Fallo genérico HTTP
    
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.printf("[MultipartSender] Initiating HTTP POST request to: %s\n", apiUrl.c_str());
    #endif

    http.setReuse(false); // No reutilizar conexiones
    if (http.begin(apiUrl)) {
        http.setTimeout(CAPTURE_DATA_HTTP_REQUEST_TIMEOUT);
        http.addHeader("Connection", "close");
        // Cabecera clave que define el tipo multipart y el boundary
        http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        
        if (!accessToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + accessToken);
        }
        
        // Enviar la petición POST con el puntero al vector de bytes y su tamaño
        httpResponseCode = http.POST(const_cast<uint8_t*>(payload.data()), payload.size());

        #ifdef ENABLE_DEBUG_SERIAL
            if (httpResponseCode > 0) {
                Serial.printf("  HTTP Response Code: %d\n", httpResponseCode);
            } else {
                Serial.printf("  HTTP POST failed, client error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            }
        #endif
        http.end(); // Liberar recursos
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.printf("[MultipartSender Error] Unable to begin HTTP connection to: %s\n", apiUrl.c_str());
        #endif
        httpResponseCode = -17; // Error cliente: http.begin() falló
    }
    return httpResponseCode;
}