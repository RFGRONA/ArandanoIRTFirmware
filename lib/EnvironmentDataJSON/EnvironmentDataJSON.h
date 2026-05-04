/**
 * @file EnvironmentDataJSON.h
 * @brief Define una clase de utilidad para formatear y enviar datos
 * de sensores ambientales como JSON vía HTTP POST.
 */
#ifndef ENVIRONMENT_DATA_JSON_H
#define ENVIRONMENT_DATA_JSON_H

#include <ArduinoJson.h> // Requerido para formatear JSON (v6+)
#include <HTTPClient.h>  // Requerido para peticiones HTTP en ESP32

/**
 * @class EnvironmentDataJSON
 * @brief Clase de utilidad (utility) que contiene un método estático
 * para construir y enviar los datos ambientales.
 */
class EnvironmentDataJSON {
public:
    /**
     * @brief Construye un JSON y envía datos ambientales a una API vía HTTP POST.
     *
     * Este método estático toma las lecturas de los sensores, las formatea
     * en un objeto JSON y envía este JSON como el cuerpo de una petición POST.
     *
     * @param fullEnvDataUrl URL completa (String) del endpoint de la API.
     * @param accessToken Token de acceso (String) para la cabecera 'Authorization'.
     * @param timestamp Hora de captura en formato ISO 8601 (String).
     * @param lightLevel Nivel de luz ambiental (float, lux).
     * @param temperature Temperatura (float, °C).
     * @param humidity Humedad relativa (float, %).
     * @param pressure Presión barométrica (float, hPa).
     *
     * @return El código de estado HTTP devuelto por el servidor (ej. 200, 401, 500).
     * Retorna un valor negativo si ocurre un error en el cliente
     * (ej. -1 URL vacía, -2 sin WiFi, -5 fallo de conexión).
     */
    static int IOEnvironmentData(
        const String& fullEnvDataUrl,
        const String& accessToken,
        const String& timestamp,
        float lightLevel,
        float temperature,
        float humidity,
        float pressure 
    );
};

#endif // ENVIRONMENT_DATA_JSON_H