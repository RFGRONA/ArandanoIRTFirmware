/**
 * @file MultipartDataSender.h
 * @brief Define una clase de utilidad para enviar datos térmicos (JSON) e
 * imágenes (JPEG) como una petición POST 'multipart/form-data'.
 *
 * Proporciona un método estático para empaquetar lecturas de sensores térmicos
 * (datos crudos, max, min, prom) y una imagen (binario JPEG) en una
 * única petición HTTP POST.
 */
#ifndef MULTIPART_DATA_SENDER_H
#define MULTIPART_DATA_SENDER_H

#include <HTTPClient.h>      
#include <WiFi.h>            
#include <ArduinoJson.h>     // Requerido para crear el JSON de datos térmicos
#include <vector>            // Requerido para std::vector (construcción del payload)
#include <math.h>            // Requerido para INFINITY, NAN, isnan

/**
 * @class MultipartDataSender
 * @brief Clase de utilidad (estática) para enviar datos combinados (térmicos + imagen).
 *
 * Maneja la complejidad de formatear una petición multipart/form-data,
 * incluyendo las cabeceras y los "boundaries" (límites).
 * No necesita ser instanciada (solo métodos estáticos).
 */
class MultipartDataSender {
public:

    // --- Constantes ---
    static const int THERMAL_WIDTH = 32; ///< Ancho del array del sensor térmico.
    static const int THERMAL_HEIGHT = 24; ///< Alto del array del sensor térmico.
    static const int THERMAL_PIXELS = THERMAL_WIDTH * THERMAL_HEIGHT; ///< Total de píxeles térmicos.

    /**
     * @brief Envía datos térmicos y una imagen JPEG vía HTTP POST (multipart/form-data).
     *
     * Construye un payload multipart con dos partes:
     * 1. 'thermal' (tipo JSON): Contiene "max_temp", "min_temp", "avg_temp",
     * "timestamp" y el array de "temperatures".
     * 2. 'image' (tipo image/jpeg): Contiene los datos binarios de la imagen JPEG.
     * (Esta parte es opcional; si jpegImage es null, no se incluye).
     *
     * @param fullCaptureDataUrl URL completa (String) del endpoint de la API.
     * @param accessToken Token de acceso (String) para la cabecera `Authorization`.
     * @param timestamp Timestamp (String) en formato ISO 8601 para incluir en el JSON.
     * @param thermalData Puntero al array (float[768]) de lecturas térmicas.
     * @param jpegImage Puntero al buffer (uint8_t*) de la imagen JPEG. (Opcional, puede ser nullptr).
     * @param jpegLength Tamaño (size_t) de los datos de la imagen JPEG. (Ignorado si jpegImage es nullptr).
     *
     * @return El código de estado HTTP del servidor. Retorna un valor negativo
     * si ocurre un error del lado del cliente (ej. -11, -12, -13, etc.).
     */
     static int IOThermalAndImageData( 
        const String& fullCaptureDataUrl,
        const String& accessToken,
        const String& timestamp,
        float* thermalData,
        uint8_t* jpegImage,
        size_t jpegLength
    );

    /**
     * @brief Crea un string JSON con estadísticas térmicas y el array de datos crudos.
     * @param timestamp Timestamp (String) ISO 8601.
     * @param thermalData Puntero al array (float[768]) de temperaturas.
     * @return String con el JSON formateado. Retorna String vacío si falla el cálculo o la serialización.
     */
    static String createThermalJson(const String& timestamp, float* thermalData);


    // --- Funciones de Ayuda (Helpers) para Cálculo de Datos Térmicos ---

    /**
     * @brief Calcula la temperatura máxima del array de datos térmicos.
     * Ignora valores NaN (Not a Number) durante el cálculo.
     * @param thermalData Puntero al array (float[768]) de temperaturas.
     * @return La temperatura máxima válida. Retorna -INFINITY si todos los valores son NaN.
     */
    static float calculateMaxTemperature(float* thermalData);

    /**
     * @brief Calcula la temperatura mínima del array de datos térmicos.
     * Ignora valores NaN durante el cálculo.
     * @param thermalData Puntero al array (float[768]) de temperaturas.
     * @return La temperatura mínima válida. Retorna INFINITY si todos los valores son NaN.
     */
    static float calculateMinTemperature(float* thermalData);

    /**
     * @brief Calcula la temperatura promedio del array de datos térmicos.
     * Ignora valores NaN. El promedio se basa solo en los píxeles válidos (no-NaN).
     * @param thermalData Puntero al array (float[768]) de temperaturas.
     * @return La temperatura promedio. Retorna NAN si todos los valores son NaN.
     */
    static float calculateAverageTemperature(float* thermalData);

private:

    /**
     * @brief Construye el payload completo (multipart/form-data) como un vector de bytes.
     * Ensambla las cabeceras, boundaries, JSON y datos binarios de la imagen.
     * @param boundary El string de límite (boundary) único para separar las partes.
     * @param thermalJson El string JSON (creado por `createThermalJson`).
     * @param jpegImage Puntero al buffer de la imagen JPEG (o nullptr).
     * @param jpegLength Tamaño de la imagen JPEG.
     * @return `std::vector<uint8_t>` con el payload formateado. Retorna vector vacío si hay error.
     */
    static std::vector<uint8_t> buildMultipartPayload(const String& boundary, const String& thermalJson, uint8_t* jpegImage, size_t jpegLength);

    /**
     * @brief Realiza la petición HTTP POST enviando el payload construido.
     * Maneja el establecimiento de las cabeceras `Content-Type` y `Authorization`.
     * @param apiUrl La URL de destino.
     * @param accessToken El token de acceso (String) para `Authorization`.
     * @param boundary El string de límite (boundary) (requerido para el Content-Type).
     * @param payload El payload completo (vector de bytes).
     * @return El código de estado HTTP del servidor (o código de error negativo del cliente).
     */
    static int performHttpPost( 
        const String& apiUrl,
        const String& accessToken, 
        const String& boundary,
        const std::vector<uint8_t>& payload
    );

};

#endif // MULTIPART_DATA_SENDER_H