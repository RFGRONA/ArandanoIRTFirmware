/**
 * @file OV2640Sensor.cpp
 * @brief Implementa la clase wrapper OV2640Sensor usando el driver de cámara ESP-IDF.
 */
#include "OV2640Sensor.h"
#include "esp_camera.h"   // Header principal del driver de cámara de ESP-IDF

// --- Configuración de Pines Hardware ---
// Definición de pines para la placa ESP32-S3 específica del proyecto.
// ¡Modificar estos pines según las conexiones de hardware específicas!
#define PWDN_GPIO_NUM     -1 ///< Pin Power Down (-1 = no usado).
#define RESET_GPIO_NUM    -1 ///< Pin Reset (-1 = no usado).
#define XCLK_GPIO_NUM     15 ///< Pin de Reloj Externo (XCLK).
#define SIOD_GPIO_NUM     4  ///< Pin de Datos SCCB (I2C) (SIOD).
#define SIOC_GPIO_NUM     5  ///< Pin de Reloj SCCB (I2C) (SIOC).

// Pines de Datos de la Cámara (Interfaz paralela de 8 bits)
#define Y9_GPIO_NUM       16 ///< Pin de Bus de Datos 7 (Y9/D7).
#define Y8_GPIO_NUM       17 ///< Pin de Bus de Datos 6 (Y8/D6).
#define Y7_GPIO_NUM       18 ///< Pin de Bus de Datos 5 (Y7/D5).
#define Y6_GPIO_NUM       12 ///< Pin de Bus de Datos 4 (Y6/D4).
#define Y5_GPIO_NUM       10 ///< Pin de Bus de Datos 3 (Y5/D3).
#define Y4_GPIO_NUM       8  ///< Pin de Bus de Datos 2 (Y4/D2).
#define Y3_GPIO_NUM       9  ///< Pin de Bus de Datos 1 (Y3/D1).
#define Y2_GPIO_NUM       11 ///< Pin de Bus de Datos 0 (Y2/D0).

// Pines de Sincronización de la Cámara
#define VSYNC_GPIO_NUM    6  ///< Pin de Sincronización Vertical (VSYNC).
#define HREF_GPIO_NUM     7  ///< Pin de Referencia Horizontal (HREF).
#define PCLK_GPIO_NUM     13 ///< Pin de Reloj de Píxel (PCLK).
// --------------------------------

/**
 * @brief Constructor por defecto.
 */
OV2640Sensor::OV2640Sensor() {
    // Intencionalmente vacío. La inicialización ocurre en begin().
}

/**
 * @brief Inicializa el hardware y el driver de la cámara.
 */
bool OV2640Sensor::begin() {
    // Estructura de configuración requerida por esp_camera_init.
    camera_config_t config;

    // --- Configuración de Pines ---
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;

    // --- Configuración de Reloj, Formato, Tamaño y Buffers ---
    config.xclk_freq_hz = 20000000;       // Frecuencia del reloj externo (ej. 20MHz).
    config.ledc_channel = LEDC_CHANNEL_0; // Canal LEDC para generar XCLK.
    config.ledc_timer = LEDC_TIMER_0;     // Timer LEDC para XCLK.

    config.pixel_format = PIXFORMAT_JPEG; // Formato de píxel deseado (JPEG para uso directo).
    config.frame_size = FRAMESIZE_VGA;    // Tamaño del fotograma (640x480).
    config.jpeg_quality = 10;             // Calidad JPEG (0-63). Menor número = mayor calidad, mayor tamaño.

    // Configuración de Framebuffers
    config.fb_count = 1;                  // Número de framebuffers. 1 = la captura se detiene hasta que el buffer se retorna.
    config.fb_location = CAMERA_FB_IN_PSRAM; // Almacenar framebuffers en PSRAM (esencial para JPEG/VGA).
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Estrategia de captura: capturar cuando el buffer esté libre.

    // --- Inicializar el Driver de la Cámara ---
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        // Falló la inicialización.
        // (Habilitar Serial.printf para ver el código de error ESP-IDF).
        // Serial.printf("Camera Init Failed with error 0x%x (%s)\n", err, esp_err_to_name(err));
        return false;
    }

    // Cámara inicializada exitosamente.
    // (Opcional: configurar brillo, contraste, etc. aquí).
    return true;
}

/**
 * @brief Captura un fotograma JPEG en un buffer recién alocado.
 */
uint8_t* OV2640Sensor::captureJPEG(size_t &length) {
    // Reinicia el parámetro de salida 'length'.
    length = 0;

    // 1. Adquirir un framebuffer del driver que contiene la imagen capturada.
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
#ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[OV2640Sensor] CRITICAL: esp_camera_fb_get() returned NULL!");
#endif
        return nullptr; // Falla al obtener el frame.
    }

    // 2. Verificar que el formato sea JPEG (como se configuró en begin).
    if(fb->format != PIXFORMAT_JPEG){
#ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[OV2640Sensor] Camera capture failed: Unexpected pixel format %d\n", fb->format);
#endif
        // ¡Importante! Retornar el buffer al driver aunque el formato sea incorrecto.
        esp_camera_fb_return(fb); 
        return nullptr;
    }

    // (Los logs de depuración sobre PSRAM se omiten para claridad).

    // 3. Alojar memoria en PSRAM para la *copia* de los datos JPEG.
    // Se usa ps_malloc para asegurar que la memoria esté en PSRAM.
    uint8_t *jpegData = (uint8_t *)ps_malloc(fb->len);
    if (!jpegData) {
#ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[OV2640Sensor] CRITICAL: Failed to allocate %u bytes in PSRAM!\n", (unsigned int)fb->len);
#endif
        // ¡Crítico! Se debe retornar el framebuffer al driver aunque falle la alocación de memoria.
        esp_camera_fb_return(fb);
        return nullptr; // Falla de alocación de memoria.
    }

    // 4. Copiar los datos desde el buffer del driver (fb->buf) a nuestro buffer (jpegData).
    memcpy(jpegData, fb->buf, fb->len);
    length = fb->len; // Asignar el tamaño de salida (bytes copiados).

    // 5. Retornar el framebuffer *original* del driver para que pueda ser reutilizado.
    // Esto es absolutamente crítico; si no se hace, el driver se quedará sin buffers.
    esp_camera_fb_return(fb);

    // 6. Retornar el puntero a nuestra *copia* de los datos JPEG en PSRAM.
    // El llamador (caller) es ahora responsable de esta memoria.
    return jpegData;
}

/**
 * @brief Desinicializa el driver de la cámara, liberando recursos.
 */
void OV2640Sensor::end() {
    // Llama a la función de ESP-IDF para apagar correctamente el driver
    // y liberar los recursos hardware (I2S, DMA, pines de cámara).
    esp_camera_deinit();
}