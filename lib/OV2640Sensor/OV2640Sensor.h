/**
 * @file OV2640Sensor.h
 * @brief Define una clase "wrapper" (envoltorio) para el módulo de cámara OV2640
 * usando el driver ESP-IDF.
 *
 * Proporciona métodos para inicializar la cámara (con pines predefinidos
 * para la placa ESP32-S3 del proyecto), capturar imágenes JPEG y
 * desinicializar la cámara para liberar recursos.
 */
#ifndef OV2640SENSOR_H
#define OV2640SENSOR_H

#include <Arduino.h>  
// Requerido para funciones de alocación de memoria (ej. ps_malloc en PSRAM)
#include "esp_heap_caps.h"   

/**
 * @class OV2640Sensor
 * @brief Clase wrapper que simplifica la interacción con la cámara OV2640
 * vía el driver ESP-IDF.
 *
 * Encapsula la lógica de inicialización, captura de imagen (JPEG) y
 * desinicialización, usando configuraciones de pines específicas definidas
 * en el archivo .cpp.
 */
class OV2640Sensor {
public:
    /**
     * @brief Constructor por defecto.
     * No inicializa el hardware; se debe llamar a begin() para eso.
     */
    OV2640Sensor();

    /**
     * @brief Inicializa el módulo de cámara OV2640 usando el driver ESP-IDF.
     *
     * Configura la cámara basándose en la definición de pines y ajustes
     * (frecuencia, formato JPEG, tamaño VGA, buffers en PSRAM)
     * especificados en la implementación (.cpp).
     * Llama internamente a `esp_camera_init`.
     *
     * @return `true` si la inicialización fue exitosa, `false` en caso contrario.
     */
    bool begin();

    /**
     * @brief Captura un único fotograma (frame) de imagen JPEG.
     *
     * Esta función sigue un flujo crítico de gestión de memoria:
     * 1. Obtiene el framebuffer del driver (`esp_camera_fb_get`).
     * 2. Aloja un *nuevo* buffer en PSRAM (`ps_malloc`) del tamaño exacto.
     * 3. Copia los datos JPEG del buffer del driver al nuevo buffer.
     * 4. Retorna el framebuffer *original* al driver (`esp_camera_fb_return`)
     * para que pueda ser reutilizado.
     *
     * @param[out] length Referencia a un size_t que se llenará con el tamaño
     * (en bytes) de la imagen JPEG capturada. Se establece en 0 si falla.
     *
     * @return Puntero (uint8_t*) al *nuevo* buffer alocado en PSRAM que
     * contiene los datos JPEG.
     * **El llamador (caller) es responsable de liberar esta memoria**
     * usando `free()` (o `ps_free()`) cuando ya no la necesite.
     * Retorna `nullptr` si la captura o la alocación de memoria fallan.
     */
    uint8_t* captureJPEG(size_t &length);

    /**
     * @brief Desinicializa el driver de la cámara y libera los recursos hardware.
     *
     * Llama internamente a `esp_camera_deinit()`. Esto es crucial para liberar
     * recursos (DMA, I2S, buffers) antes de entrar en deep sleep o
     * reconfigurar la cámara.
     */
    void end();

private:
    // La configuración y estado de la cámara son gestionados internamente
    // por las funciones del driver ESP-IDF (esp_camera).
};

#endif // OV2640SENSOR_H