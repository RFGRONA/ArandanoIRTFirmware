// src/CycleController.cpp
#include "CycleController.h"
#include <LittleFS.h>     
#include "esp_sleep.h"   
#include "ErrorLogger.h"

/**
 * @brief Ensures WiFi is connected, using WiFiManager and blocking with a timeout.
 * This replicates the logic from the original main.cpp's ensureWiFiConnected.
 * @param wifiMgr The WiFiManager instance.
 * @param sysLed Reference to LEDStatus for status indication.
 * @param timeoutMs Timeout in milliseconds for connection.
 * @return `true` if connected.
 */
bool ensureWiFiConnected_Ctrl(WiFiManager& wifiMgr, LEDStatus& sysLed, unsigned long timeoutMs) {
    if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
        return true;
    }

    if (wifiMgr.getConnectionStatus() != WiFiManager::CONNECTING) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[Ctrl] WiFi is not connected. Initiating connection attempt...");
        #endif
        wifiMgr.connectToWiFi();
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[Ctrl] WiFi connection already in progress...");
         #endif
    }

    sysLed.setState(CONNECTING_WIFI);
    unsigned long startTime = millis();

    while (millis() - startTime < timeoutMs) {
        wifiMgr.handleWiFi(); // Allow WiFiManager to process its state

        if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[Ctrl] WiFi connection established successfully within timeout.");
            #endif
            // sysLed.setState(ALL_OK); // Caller (main loop) should handle state after this returns
            return true;
        }
        if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[Ctrl] WiFi connection failed permanently (max retries reached by WiFiManager).");
            #endif
            break; 
        }
        delay(100); // Small delay
    }

    #ifdef ENABLE_DEBUG_SERIAL
        if (wifiMgr.getConnectionStatus() != WiFiManager::CONNECTION_FAILED) {
             Serial.printf("[Ctrl] WiFi connection attempt timed out after %lu ms.\n", timeoutMs);
        }
    #endif
    sysLed.setState(ERROR_WIFI);
    return false;
}

/**
 * @brief Performs a simple visual blink sequence using the status LED.
 * Sequence: OK -> OFF -> OK -> OFF -> OK -> OFF
 * @param sysLed Reference to LEDStatus.
 */
void ledBlink_Ctrl(LEDStatus& sysLed) {
    // Assuming LEDState::ALL_OK and LEDState::OFF are defined
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
}

/**
 * @brief Handles API authentication and activation.
 * Logs results including internal temperature and humidity.
 * @param cfg Reference to the application's configuration.
 * @param api_obj Reference to the API communication object.
 * @param status_led Reference to the LEDStatus object for visual feedback.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return True if API is ready (activated and authenticated), false otherwise.
 */
bool handleApiAuthenticationAndActivation_Ctrl(Config& cfg, API& api_obj, LEDStatus& status_led, float internalTempForLog, float internalHumForLog) { 
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    if (!api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl_API] Device not activated. Attempting activation..."));
        #endif
        status_led.setState(CONNECTING_WIFI); 

        int activationHttpCode = api_obj.performActivation(String(cfg.deviceId), cfg.activationCode);

        if (activationHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[Ctrl_API] Activation successful (HTTP 200)."));
            #endif
            ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, 
                                 "Device activated successfully. DeviceID: " + String(cfg.deviceId), 
                                 internalTempForLog, internalHumForLog); 
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[Ctrl_API] Activation failed. HTTP Code: %d\n", activationHttpCode);
            #endif
            status_led.setState(ERROR_AUTH);
            ErrorLogger::sendLog(logUrl, "", LOG_TYPE_ERROR, 
                                 "Device activation failed. HTTP Code: " + String(activationHttpCode) + ", DeviceID: " + String(cfg.deviceId), 
                                 internalTempForLog, internalHumForLog); 
            return false;
        }
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl_API] Device activated. Performing backend and auth check..."));
    #endif
    status_led.setState(CONNECTING_WIFI); 

    int authHttpCode = api_obj.checkBackendAndAuth();

    if (authHttpCode == 200) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl_API] Backend & Auth check successful (HTTP 200)."));
        #endif

        return true;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[Ctrl_API] Backend & Auth check failed. HTTP Code: %d\n", authHttpCode);
        #endif
        status_led.setState(ERROR_AUTH); 

        String errorMessage = "Backend/Auth check failed. HTTP Code: " + String(authHttpCode);
        if (!api_obj.isActivated()) { // api_obj.checkBackendAndAuth might deactivate on critical refresh failure
            errorMessage += ". Device has been deactivated.";
        }
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             errorMessage, internalTempForLog, internalHumForLog); 
        return false;
    }
}


/**
 * @brief Frees allocated memory buffers for images and deinitializes the camera.
 * @param jpegImage Pointer to the JPEG image buffer (will be freed).
 * @param thermalData Pointer to the thermal data buffer (will be freed).
 * @param visCamera The OV2640 visual camera object to be deinitialized.
 */
/**
 * @brief Frees allocated memory buffers for JPEG image and thermal data.
 */
void cleanupImageBuffers_Ctrl(uint8_t* jpegImage, float* thermalData) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] --- Cleaning Up Image Buffers ---"));
    #endif

    if (jpegImage != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl] Freeing JPEG image buffer..."));
        #endif
        free(jpegImage);
    }

    if (thermalData != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl] Freeing thermal data buffer..."));
        #endif
        free(thermalData);
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] Image buffers freed."));
    #endif
}

/**
 * @brief Performs final actions before entering deep sleep.
 * Sets LED status based on cycle success, performs blink, and unmounts LittleFS.
 * @param cycleStatusOK Boolean indicating if the main loop cycle completed without errors.
 * @param sysLed Reference to LEDStatus.
 */
void prepareForSleep_Ctrl(bool cycleStatusOK, LEDStatus& sysLed, OV2640Sensor& visCamera) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("\n[Ctrl] --- Preparing for Deep Sleep ---");
    #endif

    if (cycleStatusOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[Ctrl] Cycle completed successfully. Setting final LED to ALL_OK.");
        #endif
        if (sysLed.getCurrentState() != ERROR_DATA && sysLed.getCurrentState() != ERROR_SENSOR &&
            sysLed.getCurrentState() != ERROR_SEND && sysLed.getCurrentState() != ERROR_AUTH &&
            sysLed.getCurrentState() != ERROR_WIFI && sysLed.getCurrentState() != TEMP_HIGH_FANS_ON) {
            sysLed.setState(ALL_OK);
        }
        delay(1000); // Show OK state or current state
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl] Cycle completed with errors. LED should reflect the last error encountered."));
        #endif
        // LED state should already be showing the last error.
        delay(3000); // Show error state
    }

    ledBlink_Ctrl(sysLed);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] Deinitializing camera peripheral before sleep..."));
    #endif
    delay(50); 
    visCamera.end(); 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] Camera deinitialized for sleep."));
    #endif
    delay(50); 

    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] Unmounted LittleFS filesystem."));
    #endif
}

/**
 * @brief Enters ESP32 deep sleep mode for a specified number of seconds.
 * Turns off the status LED (if provided) before sleeping.
 * @param seconds The duration (unsigned long) to sleep in seconds.
 * @param sysLed Optional pointer to LEDStatus to turn it off.
 */
void deepSleep_Ctrl(unsigned long seconds, LEDStatus* sysLed) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[Ctrl] Entering deep sleep for %lu seconds...\n", seconds);
        Serial.println("--------------------------------------");
        Serial.flush();
        delay(100);
    #endif
    if (sysLed != nullptr) {
        sysLed->turnOffAll();
    }

    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
}