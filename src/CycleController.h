// src/CycleController.h
#ifndef CYCLE_CONTROLLER_H
#define CYCLE_CONTROLLER_H

#include <Arduino.h>
// Include headers for types used in parameters
#include "ConfigManager.h"  
#include "WiFiManager.h"   
#include "API.h"           
#include "LEDStatus.h"     
#include "OV2640Sensor.h"   
#include "SDManager.h" 
#include "TimeManager.h"

// --- Function Prototypes for CycleController ---

/**
 * @brief Ensures WiFi is connected, using WiFiManager.
 * This function might become redundant if main.cpp directly calls wifiManager.handleWiFi()
 * and then checks status before calling ensureWiFiConnected_Ctrl.
 * For now, it mirrors the blocking ensureWiFiConnected from your original main.
 * @param wifiMgr The WiFiManager instance.
 * @param sysLed Reference to LEDStatus for status indication.
 * @param timeoutMs Timeout in milliseconds for connection.
 * @return True if connected.
 */
bool ensureWiFiConnected_Ctrl(WiFiManager& wifiMgr, LEDStatus& sysLed, unsigned long timeoutMs);

/**
 * @brief Performs a simple visual blink sequence using the status LED.
 * @param sysLed Reference to LEDStatus.
 */
void ledBlink_Ctrl(LEDStatus& sysLed);

/** @brief Handles API authentication and activation, including token refresh.
 * @param sdMgr Reference to the SDManager for persistent storage.
 * @param timeMgr Reference to the TimeManager for time synchronization.
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the API object.
 * @param status_led A reference to the LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return true if the device is activated, authenticated, and ready.
 */
bool handleApiAuthenticationAndActivation_Ctrl(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, LEDStatus& status_led, float internalTempForLog, float internalHumForLog);

/** @brief Frees allocated memory buffers for JPEG image and thermal data.
 * Does NOT deinitialize the camera.
 * @param jpegImage Pointer to the JPEG image buffer (will be freed).
 * @param thermalData Pointer to the thermal data buffer (will be freed).
 */
void cleanupImageBuffers_Ctrl(uint8_t* jpegImage, float* thermalData);

#endif // CYCLE_CONTROLLER_H