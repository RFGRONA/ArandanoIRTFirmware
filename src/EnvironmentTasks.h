/**
 * @file EnvironmentTasks.h
 * @brief Define las funciones de orquestación para leer y enviar
 * los datos de los sensores ambientales (BH1750, BME280).
 */
#ifndef ENVIRONMENT_TASKS_H
#define ENVIRONMENT_TASKS_H

#include <Arduino.h>
// Inclusión de tipos de datos usados en los parámetros
#include "BH1750Sensor.h"
#include "API.h"
#include "LEDStatus.h"
#include "ConfigManager.h" 
#include "SDManager.h"    
#include "TimeManager.h"
#include "BME280Sensor.h"

// --- Prototipos de Funciones de Tareas Ambientales ---

/**
 * @brief Orquesta la lectura de todos los sensores ambientales y el envío de datos.
 *
 * Esta es la función principal del módulo. Llama a los helpers para leer
 * los sensores (con reintentos) y luego intenta enviar los datos.
 * Basado en el éxito del envío, guarda los datos en el directorio
 * 'archive' (archivo) o 'pending' (pendientes) de la SD.
 *
 * @param sdMgr Referencia al SDManager (para logs y guardado).
 * @param timeMgr Referencia al TimeManager.
 * @param cfg Referencia a la Configuración global.
 * @param api_obj Referencia al objeto API (para envío y tokens).
 * @param lightSensor Referencia al sensor BH1750.
 * @param bmeSensor Referencia al sensor BME280.
 * @param sysLed Referencia al LEDStatus.
 * @param internalTempForLog Temperatura interna para incluir en logs.
 * @return true si los datos se leyeron Y se enviaron exitosamente.
 */
bool performEnvironmentTasks_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, BH1750Sensor& lightSensor, BME280Sensor& bmeSensor, LEDStatus& sysLed, float internalTempForLog);

/**
 * @brief Lee el sensor de luz (BH1750) con lógica de reintentos.
 * @param lightSensor Referencia al sensor BH1750.
 * @param[out] lightLevel Referencia donde se almacena el nivel de luz (lux).
 * @return true si la lectura fue exitosa (>= 0.0 lux).
 */
bool readLightSensorWithRetry_Env(BH1750Sensor& lightSensor, float &lightLevel);

/**
 * @brief Lee los sensores Temp/Hum/Presión (BME280) con lógica de reintentos.
 * @param bmeSensor Referencia al sensor BME280.
 * @param[out] temperature Referencia para la temperatura (°C).
 * @param[out] humidity Referencia para la humedad (%).
 * @param[out] pressure Referencia para la presión (hPa). 
 * @return true si todas las lecturas fueron exitosas (no son NaN).
 */
bool readBmeSensorWithRetry_Env(BME280Sensor& bmeSensor, float &temperature, float &humidity, float &pressure);

/**
 * @brief Envía los datos ambientales recolectados al endpoint del servidor.
 *
 * Esta función maneja la lógica de envío, incluyendo la gestión de
 * errores de autenticación (HTTP 401), intentando un refresco de token
 * y reintentando el envío una vez si el refresco es exitoso.
 *
 * @param sdMgr Referencia al SDManager (para logs).
 * @param timeMgr Referencia al TimeManager.
 * @param cfg Referencia a la Configuración global.
 * @param api_obj Referencia al objeto API (para envío y tokens).
 * @param timestamp String del timestamp actual (para el payload).
 * @param lightLevel Valor del nivel de luz.
 * @param temperature Valor de la temperatura.
 * @param humidity Valor de la humedad.
 * @param pressure Valor de la presión.
 * @param sysLed Referencia al LEDStatus.
 * @param internalTempForLog Temperatura interna para incluir en logs.
 * @return true si los datos se enviaron exitosamente (HTTP 200 o 204).
 */
bool sendEnvironmentDataToServer_Env(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, const String& timestamp, float lightLevel, float temperature, float humidity, float pressure,LEDStatus& sysLed, float internalTempForLog); 

#endif // ENVIRONMENT_TASKS_H