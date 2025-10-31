/**
 * @file SystemInit.h
 * @brief Prototipos de funciones para la inicialización robusta del sistema (Setup).
 *
 * Define las funciones auxiliares (helpers) que se llaman desde el `setup()`
 * principal para inicializar hardware (Serie, I2C, Sensores) y servicios
 * (WiFi, NTP) de manera secuencial y robusta, con manejo de fallos críticos.
 */
#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#include <Arduino.h>
// Inclusión de tipos de datos usados en los parámetros
#include "BME280Sensor.h" 
#include "OV2640Sensor.h"
#include "BH1750Sensor.h"   
#include "MLX90640Sensor.h" 
#include "WiFiManager.h"    
#include "LEDStatus.h"      
#include "ConfigManager.h"  
#include "API.h"            
#include "SDManager.h"      
#include "TimeManager.h"
#include "CycleController.h" 
#include "SDManager.h"

// --- Prototipos de Funciones de Inicialización del Sistema ---

/**
 * @brief Inicializa el puerto Serie (Serial) para depuración
 * si `ENABLE_DEBUG_SERIAL` está definido.
 */
void initSerial_Sys();

/**
 * @brief Inicializa la comunicación I2C (Wire).
 * @param sdaPin Pin I2C SDA.
 * @param sclPin Pin I2C SCL.
 * @param frequency Frecuencia del reloj I2C (ej. 100000).
 */
void initI2C_Sys(int sdaPin, int sclPin, uint32_t frequency = 100000);

/**
 * @brief Inicializa secuencialmente todos los sensores hardware.
 *
 * @param bme Sensor BME280 (Temp, Hum, Pres).
 * @param light Sensor BH1750 (Luz) (usa I2C).
 * @param thermal Sensor MLX90640 (Térmica) (usa I2C).
 * @param visCamera Sensor OV2640 (Visual).
 * @return String vacío ("") si todos los sensores inicializaron correctamente.
 * Retorna un String con los nombres de los sensores que fallaron (ej. "BME280,MLX90640").
 */
String initializeSensors_Sys(BME280Sensor& bme, BH1750Sensor& light, MLX90640Sensor& thermal, OV2640Sensor& visCamera);

/**
 * @brief Maneja el fallo crítico durante la inicialización de sensores.
 *
 * Registra el error en la SD, desmonta LittleFS y detiene la ejecución
 * del dispositivo (entra en un bucle infinito `while(1)`).
 * Esta función *no* retorna.
 *
 * @param sdManager Referencia al SDManager (para logs).
 * @param timeManager Referencia al TimeManager (para logs).
 * @param failedSensors El String devuelto por `initializeSensors_Sys` que lista los fallos.
 */
void handleSensorInitFailure_Sys(SDManager& sdManager, TimeManager& timeManager, const String& failedSensors);

/**
 * @brief Inicializa y conecta el WiFi de forma robusta (bloqueante, con reintentos).
 *
 * Implementa una lógica de reintentos con "backoff" exponencial.
 * Si falla después de todos los reintentos, retorna `false`.
 *
 * @param wifiMgr Referencia al WiFiManager.
 * @param led Referencia al LEDStatus.
 * @param cfg Referencia a la Configuración global.
 * @param api_comm Puntero al objeto API (para establecer la MAC).
 * @param sdMgr Referencia al SDManager (para logs).
 * @param timeMgr Referencia al TimeManager (para logs).
 * @param visCamera (Parámetro obsoleto, mantenido por compatibilidad).
 * @return true si el WiFi se conectó exitosamente, false si fallaron todos los reintentos.
 */
bool initializeWiFi_Sys(WiFiManager& wifiMgr, LEDStatus& led, Config& cfg, API* api_comm, 
                        SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera);

/**
 * @brief Sincroniza la hora NTP de forma robusta (bloqueante, con reintentos).
 *
 * Asume que el WiFi ya está conectado.
 * Si falla después de todos los reintentos, es un error fatal:
 * registra el error y reinicia el dispositivo después de un largo retardo.
 * Esta función *no* retorna si falla.
 *
 * @param timeMgr Referencia al TimeManager.
 * @param sdMgr Referencia al SDManager (para logs).
 * @param api_comm Puntero al objeto API (para logs).
 * @param cfg Referencia a la Configuración global (para logs).
 * @param gmtOffset_sec Desplazamiento GMT (zona horaria).
 * @param daylightOffset_sec Desplazamiento por horario de verano.
 * @return true si la sincronización NTP fue exitosa.
 */
bool initializeNTP_Sys(TimeManager& timeMgr, SDManager& sdMgr, API* api_comm, Config& cfg,
                       long gmtOffset_sec, int daylightOffset_sec);

#endif // SYSTEM_INIT_H