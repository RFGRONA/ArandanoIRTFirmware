#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <Arduino.h> // Para String

class ErrorLogger {
public:
    /**
     * @brief Envía un mensaje de log de error al endpoint especificado.
     * 
     * Construye un JSON con la fuente del error, el mensaje y un timestamp (millis)
     * y lo envía vía HTTP POST.
     * 
     * @param apiUrl La URL completa del endpoint de logging en el backend.
     * @param errorSource Una cadena corta identificando dónde ocurrió el error (e.g., "SENSOR_INIT", "DHT_READ", "CAPTURE_FAIL").
     * @param errorMessage El mensaje detallado del error.
     * @return true si el log se envió y el servidor respondió con un código 2xx, false en caso contrario (incluyendo si no hay conexión WiFi).
     */
    static bool sendLog(const String& apiUrl, const String& errorSource, const String& errorMessage);
};

#endif // ERRORLOGGER_H