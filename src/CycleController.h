/**
 * @file CycleController.h
 * @brief Define un conjunto de funciones auxiliares (helpers) que
 * gestionan la lógica principal del ciclo de la aplicación.
 *
 * Estas funciones son llamadas desde el 'main.cpp' para organizar
 * las tareas del bucle principal (loop), como la conexión WiFi,
 * la autenticación API y la limpieza de memoria.
 */
#ifndef CYCLE_CONTROLLER_H
#define CYCLE_CONTROLLER_H

#include <Arduino.h>
// Headers de tipos usados en los parámetros
#include "ConfigManager.h"  
#include "WiFiManager.h"   
#include "API.h"           
#include "LEDStatus.h"     
#include "OV2640Sensor.h"   
#include "SDManager.h" 
#include "TimeManager.h"

// --- Prototipos de Funciones del Controlador de Ciclo ---

/**
 * @brief Asegura que el WiFi esté conectado (función bloqueante con timeout).
 *
 * Esta función replica la lógica de 'ensureWiFiConnected' del main.cpp original.
 * Llama a `wifiMgr.handleWiFi()` internamente mientras espera la conexión.
 *
 * @param wifiMgr Referencia al WiFiManager.
 * @param sysLed Referencia al LEDStatus para indicación visual.
 * @param timeoutMs Timeout máximo en milisegundos para esperar la conexión.
 * @return true si se conectó exitosamente, false si falló o hubo timeout.
 */
bool ensureWiFiConnected_Ctrl(WiFiManager& wifiMgr, LEDStatus& sysLed, unsigned long timeoutMs);

/**
 * @brief Realiza una secuencia simple de parpadeo (blink) con el LED de estado.
 * @param sysLed Referencia al LEDStatus.
 */
void ledBlink_Ctrl(LEDStatus& sysLed);

/**
 * @brief Gestiona la autenticación y activación completa contra la API.
 *
 * Maneja la activación del dispositivo (si es necesaria) y la
 * verificación/refresco de los tokens de autenticación.
 *
 * @param sdMgr Referencia al SDManager (para logs).
 * @param timeMgr Referencia al TimeManager (para logs).
 * @param cfg Referencia a la Configuración global.
 * @param api_obj Referencia al objeto API.
 * @param status_led Referencia al LEDStatus.
 * @param internalTempForLog Temperatura interna para incluir en los logs.
 * @return true si el dispositivo está activado, autenticado y listo para operar.
 */
bool handleApiAuthenticationAndActivation_Ctrl(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, LEDStatus& status_led, float internalTempForLog);

/**
 * @brief Libera los buffers de memoria (alojados con malloc/ps_malloc)
 * de la imagen JPEG y los datos térmicos.
 *
 * @note Esta función *NO* desinicializa la cámara.
 * @param jpegImage Puntero al buffer de la imagen JPEG (será liberado).
 * @param thermalData Puntero al buffer de datos térmicos (será liberado).
 */
void cleanupImageBuffers_Ctrl(uint8_t* jpegImage, float* thermalData);

#endif // CYCLE_CONTROLLER_H