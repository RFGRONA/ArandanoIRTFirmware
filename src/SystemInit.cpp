// src/SystemInit.cpp
#include "SystemInit.h"
#include <Wire.h>         // For I2C initialization
#include "esp_sleep.h"   // For deepSleep in handleSensorInitFailure_Sys
#include <LittleFS.h>     // For LittleFS.end() in handleSensorInitFailure_Sys

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
 * Unmounts LittleFS and enters deep sleep. This function does not return.
 * The LED error state should be set by the caller before invoking this function.
 * @param sleepSeconds_cfg The duration to sleep, typically from the global config.
 */
void handleSensorInitFailure_Sys(int sleepSeconds_cfg) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] CRITICAL ERROR: Sensor initialization failed. Preparing for deep sleep.");
        Serial.println("[SysInit] LED error state should have been set by caller.");
    #endif
    // Delay for LED visibility is handled by the caller if desired

    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Unmounted LittleFS.");
    #endif
    delay(500);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SysInit] Entering deep sleep for %d seconds...\n", sleepSeconds_cfg);
        Serial.flush();
        delay(100);
    #endif
    // Caller should turn off LED if needed before deep sleep, or deepSleep_Ctrl can do it.
    // For now, this function just triggers the sleep.
    esp_sleep_enable_timer_wakeup((uint64_t)sleepSeconds_cfg * 1000000ULL);
    esp_deep_sleep_start();
}