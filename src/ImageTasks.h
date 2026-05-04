/**
 * @file ImageTasks.h
 * @brief Define las funciones de orquestación para capturar y enviar
 * los datos de imagen (térmica y visual).
 */
#ifndef IMAGE_TASKS_H
#define IMAGE_TASKS_H

#include <Arduino.h>
// Inclusión de tipos de datos usados en los parámetros
#include "OV2640Sensor.h"
#include "MLX90640Sensor.h"
#include "API.h"
#include "LEDStatus.h"
#include "ConfigManager.h" 
#include "SDManager.h"   
#include "TimeManager.h"

// --- Prototipos de Funciones de Tareas de Imagen ---

/**
 * @brief Orquesta la captura, envío y archivo/guardado de los datos de imagen.
 *
 * Esta es la función principal del módulo. Decide si tomar la foto visual
 * (basado en 'lightLevel'), captura los datos, intenta enviarlos a la API,
 * y finalmente guarda los resultados en 'archive' (si tuvo éxito) o
 * 'pending' (si falló el envío) en la SD.
 *
 * @param sdMgr Referencia al SDManager (para logs y guardado).
 * @param timeMgr Referencia al TimeManager.
 * @param cfg Referencia a la Configuración global.
 * @param api_obj Referencia al objeto API (para envío y tokens).
 * @param visCamera Referencia al sensor OV2640.
 * @param thermalSensor Referencia al sensor MLX90640.
 * @param sysLed Referencia al LEDStatus.
 * @param lightLevel Nivel de luz (lux) para decidir si se captura la imagen RGB.
 * @param[out] jpegImage Puntero a un (uint8_t*) donde se almacenará el buffer de la imagen JPEG (alocado).
 * @param[out] jpegLength Referencia (size_t) donde se almacenará el tamaño del JPEG.
 * @param[out] thermalData Puntero a un (float*) donde se almacenará el buffer de datos térmicos (alocado).
 * @param internalTempForLog Temperatura interna para incluir en logs.
 * @return true si los datos se capturaron Y se enviaron exitosamente.
 */
bool performImageTasks_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed, float lightLevel, uint8_t** jpegImage, size_t& jpegLength, float** thermalData, float internalTempForLog);

/**
 * @brief Orquesta la captura de las imágenes térmica y visual.
 *
 * Llama a las funciones 'helper' para capturar los datos de ambos sensores
 * y asigna los punteros de salida. Maneja la limpieza si una captura
 * falla después de que la otra tuvo éxito (ej. falla visual, limpia térmico).
 *
 * @param[out] jpegImage Puntero a (uint8_t*) para el buffer JPEG (alocado por `captureVisualJPEG_Img`).
 * @param[out] jpegLength Referencia (size_t) para el tamaño del JPEG.
 * @param[out] thermalData Puntero a (float*) para el buffer térmico (alocado por `captureAndCopyThermalData_Img`).
 * @return true si AMBAS capturas fueron exitosas.
 */
bool captureImages_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                       uint8_t** jpegImage, size_t& jpegLength, float** thermalData,
                       Config& cfg, API& api_obj, float internalTempForLog);

/**
 * @brief Envía los datos de captura (JSON térmico y JPEG visual) al endpoint de la API.
 *
 * Utiliza `MultipartDataSender` para empaquetar los datos.
 * Maneja la lógica de reintento en caso de error 401 (token expirado),
 * intentando un refresco de token y reintentando el envío una vez.
 *
 * @note `thermalData` es mandatorio. `jpegImage` es opcional (puede ser nullptr).
 *
 * @param timestamp String del timestamp actual (para el payload).
 * @param jpegImage Puntero al buffer JPEG (o nullptr si no se capturó).
 * @param jpegLength Tamaño del JPEG (0 si no se capturó).
 * @param thermalData Puntero al buffer de datos térmicos (mandatorio).
 * @return true si los datos se enviaron exitosamente (HTTP 200-299).
 */
bool sendImageData_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, const String& timestamp, uint8_t* jpegImage, size_t jpegLength, float* thermalData, LEDStatus& sysLed, float internalTempForLog);


/**
 * @brief Captura un frame térmico y lo *copia* en un buffer de memoria nuevo.
 *
 * Lee el frame del sensor MLX90640. Si tiene éxito, aloca (con `ps_malloc`
 * si está disponible) un nuevo buffer del tamaño exacto y copia los datos.
 *
 * @param[out] thermalDataBuffer Puntero a (float*) donde se almacenará la
 * dirección del nuevo buffer alocado.
 * @return true si la captura y la copia fueron exitosas.
 * @note El llamador es responsable de liberar la memoria de `*thermalDataBuffer`.
 */
bool captureAndCopyThermalData_Img(SDManager& sdMgr, TimeManager& timeMgr, MLX90640Sensor& thermalSensor, float** thermalDataBuffer, Config& cfg, API& api_obj, float internalTempForLog);

/**
 * @brief Captura una imagen JPEG visual.
 *
 * Llama a `visCamera.captureJPEG()`, que internamente aloca la memoria
 * (en PSRAM) para el buffer de la imagen.
 *
 * @param[out] jpegImageBuffer Puntero a (uint8_t*) donde se almacenará la
 * dirección del buffer alocado por la cámara.
 * @param[out] jpegLength Referencia (size_t) para el tamaño del JPEG.
 * @return true si la captura y alocación fueron exitosas.
 * @note El llamador es responsable de liberar la memoria de `*jpegImageBuffer`.
 */
bool captureVisualJPEG_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, uint8_t** jpegImageBuffer, size_t& jpegLength, Config& cfg, API& api_obj, float internalTempForLog);

#endif // IMAGE_TASKS_H