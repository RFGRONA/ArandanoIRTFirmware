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
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the API object.
 * @param status_led A reference to the LEDStatus object.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return true if the device is activated, authenticated, and ready.
 */
bool handleApiAuthenticationAndActivation_Ctrl(Config& cfg, API& api_obj, LEDStatus& status_led, float internalTempForLog, float internalHumForLog);

/** @brief Frees allocated memory buffers for JPEG image and thermal data.
 * Does NOT deinitialize the camera.
 * @param jpegImage Pointer to the JPEG image buffer (will be freed).
 * @param thermalData Pointer to the thermal data buffer (will be freed).
 */
void cleanupImageBuffers_Ctrl(uint8_t* jpegImage, float* thermalData);

/** @brief Performs final actions before entering deep sleep.
 * Sets LED status, performs final blink, and unmounts LittleFS.
 * @param cycleStatusOK Boolean indicating if the main loop cycle completed without errors.
 * @param sysLed Reference to LEDStatus.
 * @param visCamera Reference to the OV2640 visual camera object to be deinitialized.
 */
void prepareForSleep_Ctrl(bool cycleStatusOK, LEDStatus& sysLed, OV2640Sensor& visCamera); 

/** @brief Enters ESP32 deep sleep for a specified duration.
 * Turns off LED (if provided and not null) and enables timer wakeup. This function does not return.
 * @param seconds The duration to sleep in seconds.
 * @param sysLed Optional pointer to LEDStatus to turn it off. Can be nullptr.
 */
void deepSleep_Ctrl(unsigned long seconds, LEDStatus* sysLed); 

#endif // CYCLE_CONTROLLER_H