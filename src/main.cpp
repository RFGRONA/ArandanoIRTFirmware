// src/main.cpp
/**
 * @file main.cpp
 * @brief Main application firmware for an ESP32-S3 based environmental and plant monitoring device.
 *
 * This firmware operates in a continuous (always-on) cycle using a non-blocking state-machine-like loop.
 * It initializes critical services (WiFi, NTP) and halts on failure.
 * The main loop continuously performs quick checks (e.g., fan control). When a scheduled time arrives,
 * it verifies API authentication, executes the full data collection cycle, and then schedules the next run.
 * All deep sleep logic has been removed in favor of connection stability.
 * All code, comments, and documentation are in English.
 */

// --- Core Arduino and System Includes ---
#include <Arduino.h>
#include "nvs_flash.h"
#include "esp_timer.h"
#include <time.h>
#include "esp_sntp.h"

// --- Local Libraries (Project Specific Classes from lib/) ---
#include "DHT22Sensor.h"
#include "BH1750Sensor.h"
#include "MLX90640Sensor.h"
#include "OV2640Sensor.h"
#include "LEDStatus.h"
#include "WiFiManager.h"
#include "API.h"
#include "DHT11Sensor.h"
#include "ErrorLogger.h"
#include "SDManager.h"
#include "TimeManager.h"
#include "FanController.h"

// --- Modularized Helper Files (from src/) ---
#include "ConfigManager.h"
#include "SystemInit.h"
#include "CycleController.h"
#include "EnvironmentTasks.h"
#include "ImageTasks.h"

// --- Hardware Pin Definitions ---
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 21
#define DHT_EXTERNAL_PIN 14
#define DHT11_INTERNAL_PIN 41
#define FAN_RELAY_PIN 42

// --- Fan Control Thresholds ---
#define FAN_ON_TEMP_C           20.0f
#define FAN_OFF_TEMP_C          15.0f

// --- Time & Connection Settings ---
#define AUTH_MAX_RETRIES                5
#define AUTH_RETRY_DELAY_MS             5000
#define COLOMBIA_GMT_OFFSET_SEC         (-5 * 3600L)
#define COLOMBIA_DAYLIGHT_OFFSET_SEC    0

// --- Global Variables ---
static time_t lastNtpSyncEpochTime = 0;

// --- Global Object Instances ---
SDManager sdManager;
TimeManager timeManager;
LEDStatus led;
WiFiManager wifiManager;
API* api_comm = nullptr;

DHT22Sensor dhtExternalSensor(DHT_EXTERNAL_PIN);
DHT11Sensor dhtInternalSensor(DHT11_INTERNAL_PIN);
BH1750Sensor lightSensor(Wire);
MLX90640Sensor thermalSensor(Wire);
OV2640Sensor camera;
FanController fanController(FAN_RELAY_PIN, false);

// --- State Variables ---
static time_t nextDataCollectionEpochTime = 0;
static bool continuousCoolingActive = false;
static bool sdUsageWarning90PercentSent = false;

// =========================================================================
// ===                           SETUP FUNCTION                          ===
// =========================================================================
void setup() {
    initSerial_Sys(); 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing LED..."));
    #endif
    led.begin();
    led.setState(ALL_OK);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing NVS..."));
    #endif
    esp_err_t ret_nvs = nvs_flash_init();
    if (ret_nvs == ESP_ERR_NVS_NO_FREE_PAGES || ret_nvs == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_nvs = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret_nvs);
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] NVS Initialized."));
    #endif

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing LittleFS for config.json..."));
    #endif
    if (!initFilesystem()) { 
        led.setState(ERROR_DATA); 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: LittleFS init failed. Halting."));
        #endif
        while(1) { delay(1000); }
    }
    loadConfigurationFromFile(); 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing SD Card..."));
    #endif
    if (!sdManager.begin()) {
        led.setState(ERROR_DATA); 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: SD Card init failed. Halting."));
        #endif
        while(1) { delay(1000); } 
    }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing API communication object..."));
    #endif
    api_comm = new API(sdManager, config.apiBaseUrl, config.apiActivatePath, config.apiAuthPath, config.apiRefreshTokenPath);
    if (api_comm == nullptr) {
        ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::ERROR, "API object allocation failed in setup.");
        led.setState(ERROR_AUTH); 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: API object allocation failed. Halting."));
        #endif
        delay(1800); // 30 minutes
        ESP.restart();
    }
    
    // --- ROBUST STARTUP SEQUENCE ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Executing robust WiFi startup..."));
    #endif
    initializeWiFi_Sys(wifiManager, led, config, api_comm, sdManager, timeManager, camera);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Executing robust NTP startup..."));
    #endif
    if (!initializeNTP_Sys(timeManager, sdManager, api_comm, config, COLOMBIA_GMT_OFFSET_SEC, COLOMBIA_DAYLIGHT_OFFSET_SEC)) {
        ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::ERROR, "NTP initialization failed in setup.");
        led.setState(ERROR_TIMER);
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: NTP init failed. Halting."));
        #endif
        delay(1800); // 30 minutes
        ESP.restart();
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing Internal DHT11 Sensor..."));
    #endif
    dhtInternalSensor.begin();

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing Fan Controller..."));
    #endif
    fanController.begin(); 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing I2C bus..."));
    #endif
    initI2C_Sys(I2C_SDA_PIN, I2C_SCL_PIN); 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing external sensors..."));
    #endif
    if (!initializeSensors_Sys(dhtExternalSensor, lightSensor, thermalSensor, camera)) { 
        ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::ERROR, "External sensor init failed in setup.");
        led.setState(ERROR_SENSOR);
        handleSensorInitFailure_Sys();
    }
    
    String setupCompleteMsg = "Device setup completed. Initial Time: " + timeManager.getCurrentTimestampString();
    if (api_comm && api_comm->isActivated()){
         ErrorLogger::sendLog(sdManager, timeManager, api_comm->getBaseApiUrl() + config.apiLogPath, api_comm->getAccessToken(), LOG_TYPE_INFO, setupCompleteMsg, NAN, NAN);
    } else {
         ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::INFO, setupCompleteMsg, NAN, NAN);
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--------------------------------------"));
        Serial.println(setupCompleteMsg);
        Serial.println(F("--------------------------------------"));
    #endif
}

//=========================================================================
// ===                            MAIN LOOP                              ===
// =========================================================================
void loop() {

    // --- 1. Quick, continuous checks (runs on every single loop pass) ---
    float internalTemp = dhtInternalSensor.readTemperature();
    // Fan Control remains here to be responsive.
    if (!isnan(internalTemp)) {
        if (continuousCoolingActive) {
            if (internalTemp < FAN_OFF_TEMP_C) {
                fanController.turnOff();
                continuousCoolingActive = false;
            }
        } else {
            if (internalTemp > FAN_ON_TEMP_C) {
                fanController.turnOn();
                continuousCoolingActive = true;
            }
        }
    }

    // --- 2. Timing Gate: Check if it's time to run the data collection cycle ---
    time_t currentTime = timeManager.getCurrentEpochTime();
    if (currentTime < nextDataCollectionEpochTime) {
        delay(1000); 
        return; // Not time yet, restart loop.
    }

    // --- 3. It's time to run: Execute the full data collection and maintenance cycle ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n[MainLoop] >>> Starting Data Collection Cycle <<<"));
    #endif

    ledBlink_Ctrl(led);
    led.setState(ALL_OK);
    
    // --- 3A. Backend & Auth Check with new granular logic ---
    bool proceedWithDataCollection = false; // Default to not proceeding until a valid state is confirmed.
    float internalHumForAuth = dhtInternalSensor.readHumidity();

    for (int attempt = 1; attempt <= AUTH_MAX_RETRIES; ++attempt) {
        int resultCode = 0;

        if (!api_comm->isActivated()) {
            // Activation is a critical preliminary step. If this fails, we can't proceed.
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Device not activated. Attempting activation (%d/%d)...\n", attempt, AUTH_MAX_RETRIES);
            #endif
            if (handleApiAuthenticationAndActivation_Ctrl(sdManager, timeManager, config, *api_comm, led, internalTemp, internalHumForAuth)) {
                proceedWithDataCollection = true; // Activation successful!
                break;
            }
        } else {
            // Device is activated, so we can perform the standard auth check.
            resultCode = api_comm->checkBackendAndAuth();
            
            if (resultCode >= 200 && resultCode < 300) {
                proceedWithDataCollection = true; // Success!
                break;
            }

            if (resultCode >= 500 || resultCode < 0) { // Server is down or unreachable.
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[MainLoop] Backend appears offline. Proceeding to collect data for pending queue.");
                #endif
                proceedWithDataCollection = true; // Per new logic, we proceed anyway.
                break; // Don't retry if server is down, just break and continue the cycle.
            }
            
            // Any other error (likely 4xx) is a critical auth failure that requires retrying.
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Auth check failed with client-side error (Code: %d). Retrying (%d/%d)...\n", resultCode, attempt, AUTH_MAX_RETRIES);
            #endif
        }

        // If we are here, it means a critical auth/activation error occurred, and we should retry.
        if (attempt < AUTH_MAX_RETRIES) {
            delay(AUTH_RETRY_DELAY_MS);
        }
    }

    // --- 3B. Execute or Skip Cycle based on Auth Check ---
    if (!proceedWithDataCollection) {
        // This only happens if activation or a critical auth check (like 4xx) fails all retries.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] CRITICAL: Cannot authenticate or activate. Skipping cycle and retrying later."));
        #endif
        ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::ERROR, "Critical Auth/Activation failed. Cycle skipped.");
        led.setState(ERROR_AUTH);
        
        // Schedule next attempt after the standard interval
        unsigned long intervalMinutes = api_comm->getDataCollectionTimeMinutes() > 0 ? api_comm->getDataCollectionTimeMinutes() : config.data_interval_minutes;
        nextDataCollectionEpochTime = timeManager.getCurrentEpochTime() + (intervalMinutes * 60);
        return; // Skip the rest of this cycle.
    }
    
    // --- 3C. Data Capture ---
    float lightLevel = lightSensor.readLightLevel();

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MainLoop] Current Time: %s\n", timeManager.getCurrentTimestampString().c_str());
    #endif

    bool cycleStatusOK = true;

    // perform...Tasks functions will internally handle failed sends by saving to pending.
    if (!performEnvironmentTasks_Env(sdManager, timeManager, config, *api_comm, lightSensor, dhtExternalSensor, led, internalTemp, internalHumForAuth)) {
        cycleStatusOK = false;
    }
    
    uint8_t* localJpegImage = nullptr;
    size_t localJpegLength = 0;
    float* localThermalData = nullptr;
    if (cycleStatusOK) {
        if (!performImageTasks_Img(sdManager, timeManager, config, *api_comm, camera, thermalSensor, led, 
                                               lightLevel,
                                               &localJpegImage, localJpegLength, &localThermalData, internalTemp, internalHumForAuth)) {
            cycleStatusOK = false;
        }
    }
    
    // --- 3D. End-of-Cycle Signaling & Cleanup ---
    const char* logType = cycleStatusOK ? LOG_TYPE_INFO : LOG_TYPE_WARNING;
    String logMessage = cycleStatusOK ? "Main data cycle completed successfully." : "Main data cycle completed with errors.";
    ErrorLogger::sendLog(sdManager, timeManager, api_comm->getBaseApiUrl() + config.apiLogPath, api_comm->getAccessToken(), logType, logMessage, internalTemp, internalHumForAuth);
    
    ledBlink_Ctrl(led);
    led.setState(OFF);

    // --- 3E. Maintenance Tasks ---
    cleanupImageBuffers_Ctrl(localJpegImage, localThermalData);

    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
        sdManager.processPendingApiCalls(*api_comm, timeManager, config, internalTemp, internalHumForAuth);
    }
    
    if (sdManager.isSDAvailable()) {
        sdManager.manageAllStorage(timeManager); 
        
        uint64_t sdUsed, sdTotal;
        float usagePercent = sdManager.getUsageInfo(sdUsed, sdTotal);
        if (usagePercent >= 90.0f && !sdUsageWarning90PercentSent) {
            String msg = "CRITICAL WARNING: SD Card usage is at " + String(usagePercent, 1) + "%";
            ErrorLogger::sendLog(sdManager, timeManager, api_comm->getBaseApiUrl() + config.apiLogPath, api_comm->getAccessToken(), LOG_TYPE_WARNING, msg, internalTemp, internalHumForAuth);
            sdUsageWarning90PercentSent = true;
        } else if (usagePercent < 85.0f && sdUsageWarning90PercentSent) {
            String msg = "INFO: SD Card usage is now " + String(usagePercent, 1) + "%. Warning resolved.";
            ErrorLogger::sendLog(sdManager, timeManager, api_comm->getBaseApiUrl() + config.apiLogPath, api_comm->getAccessToken(), LOG_TYPE_INFO, msg, internalTemp, internalHumForAuth);
            sdUsageWarning90PercentSent = false;
        }
    }

    // --- 3F. NTP Sync Check ---
    time_t currentTimeForSyncCheck = timeManager.getCurrentEpochTime();
    
    // Check if more than 1 hour has passed since the last NTP sync check
    if ((currentTimeForSyncCheck - lastNtpSyncEpochTime) > 3600) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[TimeManager] WARNING: NTP sync has been lost! Attempting to re-initialize..."));
            #endif
            led.setState(ERROR_TIMER);
            initializeNTP_Sys(timeManager, sdManager, api_comm, config, COLOMBIA_GMT_OFFSET_SEC, COLOMBIA_DAYLIGHT_OFFSET_SEC);

        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[TimeManager] Periodic NTP check: Sync status is OK."));
            #endif
        }

        lastNtpSyncEpochTime = timeManager.getCurrentEpochTime();
    }

    
    // --- 4. Schedule the NEXT data collection cycle ---
    unsigned long intervalMinutes = api_comm->getDataCollectionTimeMinutes();
    if (intervalMinutes == 0) {
        intervalMinutes = config.data_interval_minutes;
    }

    time_t lastRunTime = timeManager.getCurrentEpochTime(); 
    struct tm timeinfo;
    localtime_r(&lastRunTime, &timeinfo);

    int minutes_past_slot = timeinfo.tm_min % intervalMinutes;
    int minutes_to_add = (minutes_past_slot == 0) ? intervalMinutes : (intervalMinutes - minutes_past_slot);
    
    time_t next_run_base_time = lastRunTime + (minutes_to_add * 60);

    localtime_r(&next_run_base_time, &timeinfo);
    timeinfo.tm_sec = 0;
    nextDataCollectionEpochTime = mktime(&timeinfo);
    
    if (nextDataCollectionEpochTime <= lastRunTime) {
        nextDataCollectionEpochTime += (intervalMinutes * 60);
    }

    #ifdef ENABLE_DEBUG_SERIAL
        char time_buf[50];
        localtime_r(&nextDataCollectionEpochTime, &timeinfo);
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        Serial.printf("[MainLoop] <<< Cycle Complete >>> Next run scheduled for: %s\n\n", time_buf);
    #endif
}

