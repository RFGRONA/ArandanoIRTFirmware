// src/main.cpp
/**
 * @file main.cpp
 * @brief Main application firmware for an ESP32-S3 based environmental and plant monitoring device.
 *
 * This firmware initializes various sensors, loads configuration, connects to WiFi,
 * periodically collects and sends data, manages internal temperature via fans,
 * and enters deep sleep. It utilizes modularized functions for better organization.
 */

// --- Core Arduino and System Includes ---
#include <Arduino.h>
#include "esp_sleep.h"

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

// --- New Modularized Helper Files (from src/) ---
#include "ConfigManager.h"
#include "SystemInit.h"
#include "CycleController.h"
#include "EnvironmentTasks.h"
#include "ImageTasks.h"
#include "FanController.h"  // For fan control

// --- Hardware Pin Definitions ---
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 21
#define DHT_EXTERNAL_PIN 14     // External DHT22 sensor pin
#define DHT11_INTERNAL_PIN 41   // Internal DHT11 sensor pin
#define FAN_RELAY_PIN 42        // Relay control pin for fans

// --- Sleep Durations & Delays ---
#define FAN_ON_ACTIVE_MONITOR_DELAY_MS 5000 // 5 seconds between temp checks when fans are actively cooling
#define MIN_SLEEP_S 15                        // Minimum sleep duration to prevent rapid loops

// --- Global Object Instances ---
// 'config' is defined in ConfigManager.cpp and declared extern in ConfigManager.h

BH1750Sensor lightSensor(Wire);
MLX90640Sensor thermalSensor(Wire);
DHT22Sensor dhtExternalSensor(DHT_EXTERNAL_PIN);
OV2640Sensor camera;
LEDStatus led;
WiFiManager wifiManager;
API* api_comm = nullptr;

DHT11Sensor dhtInternalSensor(DHT11_INTERNAL_PIN);
FanController fanController(FAN_RELAY_PIN, false);

// --- RTC Data for Sleep Management ---
RTC_DATA_ATTR uint32_t nextMainDataCollectionDueTimeS_RTC = 0;

// --- State Variables for Active Ventilation (Non-RTC, reset on each boot) ---
bool activeVentilationInProgress = false;
uint32_t ventilationStartUptimeS = 0;

// =========================================================================
// ===                           SETUP FUNCTION                          ===
// =========================================================================
void setup() {
    led.begin();
    led.setState(ALL_OK);

    initSerial_Sys(); // Inicializa Serial para debug

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing Filesystem..."));
    #endif
    if (!initFilesystem()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: Filesystem initialization failed. Halting."));
        #endif
        led.setState(ERROR_DATA);
        while(1) { delay(1000); }
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Loading configuration..."));
    #endif
    loadConfigurationFromFile();

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[MainSetup] Setting WiFi credentials in WiFiManager for SSID: " + config.wifi_ssid);
    #endif
    wifiManager.setCredentials(config.wifi_ssid, config.wifi_pass);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing API communication object..."));
    #endif
    api_comm = new API(config.apiBaseUrl,
                       config.apiActivatePath,
                       config.apiAuthPath,
                       config.apiRefreshTokenPath);
    if (api_comm == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] CRITICAL: Failed to allocate memory for API object. Halting."));
        #endif
        led.setState(ERROR_AUTH);
        while(1) { delay(1000); }
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Obtaining MAC Address..."));
    #endif
    String macAddr = wifiManager.getMacAddress();
    if (!macAddr.isEmpty()) {
        api_comm->setDeviceMAC(macAddr);
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MainSetup] MAC Address %s set in API object.\n", macAddr.c_str());
        #endif
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] WARNING: Could not obtain MAC address."));
        #endif
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[MainSetup] Initializing WiFiManager..."));
    #endif
    wifiManager.begin();

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
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] External sensor initialization failed."));
        #endif
        led.setState(ERROR_SENSOR);
        handleSensorInitFailure_Sys(config.sleep_sec); 
    }

    if (nextMainDataCollectionDueTimeS_RTC == 0) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainSetup] RTC target for sleep calc (nextMainDataCollectionDueTimeS_RTC) is 0 (first boot)."));
         #endif
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--------------------------------------"));
        Serial.println(F("[MainSetup] Device setup completed successfully."));
        Serial.println(F("--------------------------------------"));
    #endif
    led.setState(ALL_OK);
    delay(1000);
}

// =========================================================================
// ===                            MAIN LOOP                              ===
// =========================================================================
void loop() {
    unsigned long cycleStartTimeMs = millis();
    uint32_t currentDeviceUptimeS = esp_timer_get_time() / 1000000ULL;
    uint32_t awakeTimeThisCycleS = 0;
    bool cycleStatusOK = true; 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n======================================"));
        Serial.printf("--- Starting New Cycle (Uptime: %u s) ---\n", currentDeviceUptimeS);
        Serial.printf("[MainLoopStart] Total Free PSRAM: %u bytes.\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        Serial.printf("[MainLoopStart] Largest Free Contiguous PSRAM Block: %u bytes.\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    #endif

    // --- Main Data Collection Decision ---
    bool performMainDataTasksThisCycle = false;
    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    if (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
        performMainDataTasksThisCycle = true;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] Woke up by timer, data collection is due."));
        #endif
    } else if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED && nextMainDataCollectionDueTimeS_RTC == 0) {
        performMainDataTasksThisCycle = true;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] First boot or non-deep-sleep reset (RTC=0), performing data collection."));
        #endif
    } else if (wakeup_cause != ESP_SLEEP_WAKEUP_TIMER && wakeup_cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
        performMainDataTasksThisCycle = true;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MainLoop] Woke up by other reason (%d), performing data collection.\n", wakeup_cause);
        #endif
    }

    if (wakeup_cause != ESP_SLEEP_WAKEUP_TIMER) {
        performMainDataTasksThisCycle = true;
        #ifdef ENABLE_DEBUG_SERIAL
            if (wakeup_cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
                 Serial.println(F("[MainLoop] Wakeup cause UNDEFINED (likely first boot/flash), ensuring data collection."));
            } else {
                 Serial.printf("[MainLoop] Wakeup cause %d (not timer), ensuring data collection.\n", wakeup_cause);
            }
        #endif
    }


    #ifdef ENABLE_DEBUG_SERIAL
        if (activeVentilationInProgress) {
            Serial.println(F("[MainLoop] Mode: ACTIVE VENTILATION."));
        } else {
            Serial.println(F("[MainLoop] Mode: DEEP SLEEP CYCLE."));
        }
        Serial.printf("[MainLoop] RTC Value (nextMainDataCollectionDueTimeS_RTC) for sleep calc: %u s\n", nextMainDataCollectionDueTimeS_RTC);
    #endif

    if (!activeVentilationInProgress && wakeup_cause == ESP_SLEEP_WAKEUP_TIMER) {
        ledBlink_Ctrl(led);
    }

    float currentInternalTemp = dhtInternalSensor.readTemperature();
    float currentInternalHum = dhtInternalSensor.readHumidity();
    #ifdef ENABLE_DEBUG_SERIAL
        if (isnan(currentInternalTemp)) Serial.println(F("[MainLoop] Failed to read internal temperature."));
        else Serial.printf("[MainLoop] Internal Temp: %.2f C, Hum: %.2f %%\n", currentInternalTemp, currentInternalHum);
    #endif

    if (!isnan(currentInternalTemp)) {
        fanController.controlTemperature(currentInternalTemp);
    }
    // ... (Actualización del LED basada en el estado del ventilador, como lo tenías)

    // --- Active Ventilation Mode Logic (MODIFICADA PARA PROBLEMA 1 DE PREGUNTA ANTERIOR) ---
    if (activeVentilationInProgress) {
        if (!fanController.isOn()) fanController.turnOn();
        // ... (led state) ...
        if (!isnan(currentInternalTemp) && currentInternalTemp < INTERNAL_LOW_TEMP_OFF) {
            activeVentilationInProgress = false;
            fanController.turnOff();
            // ... (led state, log, delay) ...
            // NO HAY RETURN AQUÍ, permite que continúe para posible toma de datos o cálculo de sueño
        } else { // Temperatura aún alta
            // Si performMainDataTasksThisCycle es true, las tareas de datos se ejecutarán más abajo.
            // Si NO es momento de tareas de datos (porque la ventilación está en un sub-ciclo de monitoreo),
            // entonces sí hacemos return para un ciclo corto.
            // ¿Cómo sabemos si es un sub-ciclo?
            // Por ahora, la lógica original era:
            // bool needsDataCollectionDuringVentilation = (nextMainDataCollectionDueTimeS_RTC == 0 || currentDeviceUptimeS >= nextMainDataCollectionDueTimeS_RTC);
            // Esta condición `needsDataCollectionDuringVentilation` ya no es confiable.
            // Con `performMainDataTasksThisCycle = true` al despertar, las tareas se harán.
            // El `return` aquí solo debería ocurrir si las tareas YA se hicieron en este ciclo de "despierto"
            // y solo estamos monitoreando. Esto se maneja en la sección "Post-Data Collection Ventilation Check".
            // Así que aquí, si la temperatura sigue alta, simplemente dejamos que el flujo continúe.
             #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[MainLoop] Ventilation active, temp high. Proceeding with main cycle flow."));
            #endif
        }
    } else { // Not currently in activeVentilationInProgress
        if (!isnan(currentInternalTemp) && currentInternalTemp > INTERNAL_HIGH_TEMP_ON) {
            activeVentilationInProgress = true;
            ventilationStartUptimeS = currentDeviceUptimeS;
            fanController.turnOn();
        }
    }

    // --- WiFi, API, y Toma de Datos ---
    String logUrl = api_comm ? api_comm->getBaseApiUrl() + config.apiLogPath : "";
    if (performMainDataTasksThisCycle || (activeVentilationInProgress && (nextMainDataCollectionDueTimeS_RTC == 0 || currentDeviceUptimeS >= nextMainDataCollectionDueTimeS_RTC)) ) {

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] Proceeding with WiFi/API checks..."));
        #endif
        wifiManager.handleWiFi();
        if (!ensureWiFiConnected_Ctrl(wifiManager, led, 20000)) {
            ErrorLogger::sendLog(logUrl, api_comm ? api_comm->getAccessToken() : "", LOG_TYPE_ERROR, "WiFi connection failed.", currentInternalTemp, currentInternalHum);
            prepareForSleep_Ctrl(false, led, camera);
            deepSleep_Ctrl(60, &led);
            return;
        }
        if (!handleApiAuthenticationAndActivation_Ctrl(config, *api_comm, led, currentInternalTemp, currentInternalHum)) {
            prepareForSleep_Ctrl(false, led, camera);
            unsigned long sleepApiFailS = api_comm->getDataCollectionTimeMinutes();
            if (sleepApiFailS <=0) sleepApiFailS = config.sleep_sec; else sleepApiFailS = (sleepApiFailS > 5 ? sleepApiFailS * 60 : 5 * 60);
            deepSleep_Ctrl(sleepApiFailS, &led);
            return;
        }
        // ... (actualizar LED si API OK) ...
    }


    // --- Main Data Collection Execution ---
    unsigned long mainDataIntervalS_target = api_comm->getDataCollectionTimeMinutes();
    if (mainDataIntervalS_target == 0) {
        // Asigna un valor por defecto si la API no devuelve uno o es 0
        // Necesitas definir DEFAULT_DATA_COLLECTION_MINUTES en alguna parte
        // Por ejemplo, #define DEFAULT_DATA_COLLECTION_MINUTES 5
        mainDataIntervalS_target = config.sleep_sec / 60; // O usa el sleep_sec de config
        if (mainDataIntervalS_target == 0) mainDataIntervalS_target = 1; // Asegurar al menos 1 minuto
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MainLoop] API data collection interval is 0, using default/config: %lu minutes\n", mainDataIntervalS_target);
         #endif
    }
    mainDataIntervalS_target *= 60; // Convertir a segundos


    uint8_t* localJpegImage = nullptr;
    size_t localJpegLength = 0;
    float* localThermalData = nullptr;

    // La decisión de `performMainDataTasksThisCycle` ya se tomó arriba basada en wakeup_cause
    if (performMainDataTasksThisCycle) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] --- Performing Main Data Collection Tasks ---"));
        #endif
        LEDState previousLedStateBeforeData = led.getCurrentState();

        if (!performEnvironmentTasks_Env(config, *api_comm, lightSensor, dhtExternalSensor, led, currentInternalTemp, currentInternalHum)) {
            cycleStatusOK = false;
        }
        if (cycleStatusOK) {
            if (!performImageTasks_Img(config, *api_comm, camera, thermalSensor, led,
                                       &localJpegImage, localJpegLength, &localThermalData, currentInternalTemp, currentInternalHum)) {
                cycleStatusOK = false;
            }
        }

        if (localJpegImage || localThermalData) {
            cleanupImageBuffers_Ctrl(localJpegImage, localThermalData);
            localJpegImage = nullptr;
            localThermalData = nullptr;
        }

        if (cycleStatusOK) {
            nextMainDataCollectionDueTimeS_RTC = currentDeviceUptimeS + mainDataIntervalS_target;
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Main data tasks SUCCEEDED. Next collection due at RTC uptime target: %u. (current_uptime %u + interval %lu)\n",
                              nextMainDataCollectionDueTimeS_RTC, currentDeviceUptimeS, mainDataIntervalS_target);
            #endif
        } else {
            nextMainDataCollectionDueTimeS_RTC = currentDeviceUptimeS + MIN_SLEEP_S;
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Main data tasks FAILED. Scheduling quick retry. Next collection due at RTC uptime target: %u. (current_uptime %u + min_sleep %d)\n",
                              nextMainDataCollectionDueTimeS_RTC, currentDeviceUptimeS, MIN_SLEEP_S);
            #endif
        }
        ErrorLogger::sendLog(logUrl, api_comm->getAccessToken(), cycleStatusOK ? LOG_TYPE_INFO : LOG_TYPE_WARNING,
                             cycleStatusOK ? "Main data collection cycle completed successfully." : "Main data collection cycle completed with errors.",
                             currentInternalTemp, currentInternalHum);
        // ... (Restaurar LED state) ...
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[MainLoop] Main data tasks SKIPPED this cycle based on initial decision."));
            // Si se saltaron las tareas, nextMainDataCollectionDueTimeS_RTC no se actualiza.
            // El cálculo del sueño usará el valor de RTC persistente.
        #endif
        cycleStatusOK = true; // Considerar el ciclo OK si solo se saltaron tareas programadas para no hacerse.
    }


    // --- Post-Data Collection / Post-Ventilation-Decision Ventilation Check ---
    if (activeVentilationInProgress) {
        currentInternalTemp = dhtInternalSensor.readTemperature(); // Re-leer temperatura
        if (!isnan(currentInternalTemp) && currentInternalTemp < INTERNAL_LOW_TEMP_OFF) {
            activeVentilationInProgress = false;
            fanController.turnOff();
            // ... (led state, log, delay) ...
        } else if (!isnan(currentInternalTemp)) { // Temp sigue alta
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Active ventilation CONTINUES (after data tasks or decision). Temp: %.1fC. Monitoring...\n", currentInternalTemp);
            #endif
            delay(FAN_ON_ACTIVE_MONITOR_DELAY_MS);
            return; // Loop de nuevo para ventilación activa (NO VA A DORMIR)
        }
    }

    // --- Calculate and Enter Deep Sleep ---
    unsigned long sleepDurationFinalS;
    awakeTimeThisCycleS = (millis() - cycleStartTimeMs) / 1000;

    if (nextMainDataCollectionDueTimeS_RTC > currentDeviceUptimeS) {
        sleepDurationFinalS = nextMainDataCollectionDueTimeS_RTC - currentDeviceUptimeS;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[MainLoop] RTC target %u not in future of current uptime %u or RTC is 0. Calculating default sleep.\n",
                          nextMainDataCollectionDueTimeS_RTC, currentDeviceUptimeS);
        #endif
        // Si RTC es 0 (primer boot y falló antes de fijar RTC) o si el tiempo ya pasó (currentUptime > RTC)
        // Necesitamos un plan B para el sueño.
        if (performMainDataTasksThisCycle && !cycleStatusOK) { // Si se intentaron datos y fallaron en este ciclo
            sleepDurationFinalS = MIN_SLEEP_S;
        } else { // Para otros casos (ej. primer boot sin RTC fijado, o RTC ya pasó)
                 // Dormir por el intervalo principal menos el tiempo despierto, o MIN_SLEEP_S.
            sleepDurationFinalS = mainDataIntervalS_target > awakeTimeThisCycleS ? mainDataIntervalS_target - awakeTimeThisCycleS : MIN_SLEEP_S;
        }
    }

    if (sleepDurationFinalS < MIN_SLEEP_S) {
        sleepDurationFinalS = MIN_SLEEP_S;
    }

    // Lógica de "capping" que volviste a poner (con el buffer de +1 minuto)
    if (mainDataIntervalS_target > 0 && sleepDurationFinalS > mainDataIntervalS_target) {
        if (sleepDurationFinalS > mainDataIntervalS_target + (1 * 60)) {
            sleepDurationFinalS = mainDataIntervalS_target + (1*60);
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[MainLoop] Capping sleep duration to target interval + 1 min: %lu s.\n", sleepDurationFinalS);
            #endif
        }
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[MainLoop] RTC Value for sleep calc (nextMainDataCollectionDueTimeS_RTC): %u s\n", nextMainDataCollectionDueTimeS_RTC);
        Serial.printf("[MainLoop] Awake time this cycle: %u s. Final determined sleep duration: %lu seconds.\n", awakeTimeThisCycleS, sleepDurationFinalS);
    #endif

    prepareForSleep_Ctrl(cycleStatusOK, led, camera);
    deepSleep_Ctrl(sleepDurationFinalS, &led);
}