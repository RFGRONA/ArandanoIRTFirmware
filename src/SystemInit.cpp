// src/SystemInit.cpp
#include "SystemInit.h"
#include <Wire.h>         // For I2C initialization
#include <LittleFS.h>     // For LittleFS.end() in handleSensorInitFailure_Sys
#include "ErrorLogger.h" // For error logging

// --- Definitions for robust startup routines ---
#define WIFI_SETUP_MAX_RETRIES 5
#define WIFI_SETUP_INITIAL_BACKOFF_S 4  // Initial delay between retries in seconds
#define WIFI_SETUP_MAX_BACKOFF_S 30     // Maximum delay between retries in seconds

#define NTP_SETUP_MAX_RETRIES 5
#define NTP_SETUP_RETRY_DELAY_MS 2000


/**
 * @brief Initializes the Serial port for debugging output if ENABLE_DEBUG_SERIAL is defined.
 * Starts Serial communication at 115200 baud and prints a boot/wake message
 * including the device's unique chip ID (MAC address based).
 */
void initSerial_Sys() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.begin(115200);
        // while (!Serial); // Optional: Wait for serial monitor - remove for unattended operation.
        delay(1000); // Give Serial time to initialize
        uint64_t chipid = ESP.getEfuseMac();
        Serial.printf("\n--- Device Booting / Waking Up (Chip ID: %04X%08X) ---\n",
                      (uint16_t)(chipid >> 32),
                      (uint32_t)chipid);
        Serial.println("[SysInit] Debug Serial Enabled (Rate: 115200)");
    #endif
}

/**
 * @brief Initializes I2C communication.
 * @param sdaPin The I2C SDA pin.
 * @param sclPin The I2C SCL pin.
 * @param frequency The I2C clock frequency.
 */
void initI2C_Sys(int sdaPin, int sclPin, uint32_t frequency) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SysInit] Initializing I2C Bus (Pins SDA: %d, SCL: %d, Freq: %lu Hz)...\n", sdaPin, sclPin, frequency);
    #endif
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(frequency);
    delay(100); // Delay after I2C init
}


/**
 * @brief Initializes all connected hardware sensors sequentially.
 * Assumes I2C has been initialized separately by initI2C_Sys.
 * @param dht The DHT22 sensor object.
 * @param light The BH1750 light sensor object.
 * @param thermal The MLX90640 thermal sensor object.
 * @param visCamera The OV2640 visual camera object.
 * @return `true` if all sensors were initialized successfully. `false` if any sensor failed.
 */
bool initializeSensors_Sys(DHT22Sensor& dht, BH1750Sensor& light, MLX90640Sensor& thermal, OV2640Sensor& visCamera) {
    // --- Initialize DHT22 Sensor ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing DHT22 sensor...");
    #endif
    dht.begin(); // DHT library begin() usually doesn't return a status.
    delay(100);

    // --- I2C Bus should be initialized by initI2C_Sys before calling this function ---

    // --- Initialize BH1750 Light Sensor (via I2C) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing BH1750 (Light Sensor)...");
    #endif
    if (!light.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! BH1750 Light Sensor Initialization FAILED !!!");
        #endif
        // LED error state will be set by the caller (main.cpp setup)
        return false;
    }
    delay(100);

    // --- Initialize MLX90640 Thermal Sensor (via I2C) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing MLX90640 (Thermal Sensor)...");
    #endif
    if (!thermal.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! MLX90640 Thermal Sensor Initialization FAILED !!!");
        #endif
        // LED error state will be set by the caller
        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Waiting for MLX90640 measurement stabilization (~2 seconds)...");
    #endif
    delay(2000); // Adjust based on MLX90640 refresh rate

    // --- Initialize OV2640 Camera Sensor ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing OV2640 (Visual Camera)...");
    #endif
    if (!visCamera.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! OV2640 Camera Initialization FAILED !!!");
        #endif
        // LED error state will be set by the caller
        return false;
    }
    delay(500);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] All sensors initialized successfully.");
    #endif
    return true;
}

/**
 * @brief Handles the failure scenario during sensor initialization in setup().
 * This function now halts the device in a while(1) loop instead of sleeping.
 * The LED error state should be set by the caller before invoking this function.
 */
void handleSensorInitFailure_Sys() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit] CRITICAL ERROR: Sensor initialization failed. Halting execution."));
        Serial.println(F("[SysInit] LED error state should have been set by caller."));
    #endif

    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Unmounted LittleFS.");
    #endif
    delay(500);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- SYSTEM HALTED ---"));
        Serial.flush();
    #endif
    // Halt the system. A watchdog timer will eventually reset the device.
    while(1) {
        delay(1000);
    }
}

/**
 * @brief Initializes and robustly connects to WiFi with retries and exponential backoff.
 * This function is now blocking and will halt the device on critical failure.
 * @return True if WiFi connected successfully. Halts on failure, so it doesn't return false.
 */
bool initializeWiFi_Sys(WiFiManager& wifiMgr, LEDStatus& led, Config& cfg, API* api_comm, 
                        SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit_WiFi] Initializing WiFiManager and setting credentials..."));
    #endif
    wifiMgr.begin();
    wifiMgr.setCredentials(cfg.wifi_ssid, cfg.wifi_pass);

    if (api_comm != nullptr) { 
        String macAddr = wifiMgr.getMacAddress();
        if (!macAddr.isEmpty()) {
            api_comm->setDeviceMAC(macAddr);
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_WiFi] MAC Address %s set in API object.\n", macAddr.c_str());
            #endif
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[SysInit_WiFi] WARNING: Could not obtain MAC address for API object."));
            #endif
        }
    }

    led.setState(CONNECTING_WIFI);
    unsigned long backoffDelayS = WIFI_SETUP_INITIAL_BACKOFF_S;
    
    for (int attempt = 1; attempt <= WIFI_SETUP_MAX_RETRIES; ++attempt) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_WiFi] Connection attempt #%d/%d...\n", attempt, WIFI_SETUP_MAX_RETRIES);
        #endif
        
        wifiMgr.connectToWiFi(); // This is a non-blocking call that starts the connection process

        // Wait for connection with a timeout
        unsigned long attemptStartTime = millis();
        while (millis() - attemptStartTime < WIFI_CONNECT_TIMEOUT) {
            // wifiMgr.handleWiFi() is not needed here as we use event-driven status
            if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[SysInit_WiFi] WiFi connected successfully."));
                #endif
                led.setState(ALL_OK);
                return true;
            }
            delay(100);
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_WiFi] Attempt #%d timed out after %d ms.\n", attempt, WIFI_CONNECT_TIMEOUT);
        #endif

        if (attempt < WIFI_SETUP_MAX_RETRIES) {
            led.setState(ERROR_WIFI);
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_WiFi] Waiting for %lu seconds before retrying...\n", backoffDelayS);
            #endif
            delay(backoffDelayS * 1000);
            backoffDelayS = min(backoffDelayS * 2, (unsigned long)WIFI_SETUP_MAX_BACKOFF_S); // Exponential backoff with a cap
            led.setState(CONNECTING_WIFI);
        }
    }
    
    // All retries failed, this is a critical failure.
    String errorMsg = "CRITICAL: All WiFi connection attempts failed. Halting.";
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit_WiFi] " + errorMsg);
    #endif
    led.setState(ERROR_WIFI);
    ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, errorMsg);
    
    // Halt the system
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- SYSTEM HALTED ---"));
        Serial.flush();
    #endif
    while(1) {
        delay(1000);
    }

    return false; // Should not be reached
}


/**
 * @brief Initializes and robustly syncs NTP time with retries.
 * This function is blocking and will halt the device on critical failure.
 * Assumes WiFi is already connected.
 * @return True if NTP sync was successful. Halts on failure, so it doesn't return false.
 */
bool initializeNTP_Sys(TimeManager& timeMgr, SDManager& sdMgr, API* api_comm, Config& cfg,
                       long gmtOffset_sec, int daylightOffset_sec) {
    if (WiFi.status() != WL_CONNECTED) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SysInit_NTP] FATAL: WiFi not connected. Cannot sync NTP. Halting."));
         #endif
         // This should not happen if initializeWiFi_Sys is called first and succeeds.
        delay(3600000UL); // 1 hour
        ESP.restart();
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit_NTP] Initializing TimeManager and starting NTP sync..."));
    #endif
    timeMgr.begin(DEFAULT_NTP_SERVER_1, DEFAULT_NTP_SERVER_2, gmtOffset_sec, daylightOffset_sec);
    
    for (int attempt = 1; attempt <= NTP_SETUP_MAX_RETRIES; ++attempt) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_NTP] NTP sync attempt #%d/%d...\n", attempt, NTP_SETUP_MAX_RETRIES);
        #endif
        if (timeMgr.syncNtpTime()) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SysInit_NTP] NTP time synchronized: " + timeMgr.getCurrentTimestampString());
            #endif
            // Log success to SD, not remotely, to avoid dependency loops if logging requires time.
            ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::INFO, "NTP time synchronized successfully at setup.");
            return true;
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_NTP] NTP sync attempt #%d failed.\n", attempt);
        #endif

        if (attempt < NTP_SETUP_MAX_RETRIES) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_NTP] Waiting for %d ms before retrying...\n", NTP_SETUP_RETRY_DELAY_MS);
            #endif
            delay(NTP_SETUP_RETRY_DELAY_MS);
        }
    }

    // All retries failed
    String errorMsg = "CRITICAL: All NTP time synchronization attempts failed. Halting.";
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit_NTP] " + errorMsg);
    #endif
    // At this point, time is not synced, so the timestamp in the log will be uptime-based.
    ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, errorMsg);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- SYSTEM HALTED ---"));
        Serial.flush();
    #endif
    delay(3600000UL); // 1 hour
    ESP.restart();
    
    return false; // Should not be reached
}