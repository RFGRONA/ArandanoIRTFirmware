/**
 * @file main.cpp
 * @brief Main application firmware for an ESP32-S3 based environmental and plant monitoring device.
 *
 * This firmware initializes various sensors (DHT22, BH1750, MLX90640, OV2640),
 * loads configuration settings from LittleFS, connects to a WiFi network using
 * the loaded credentials, periodically reads sensor data, captures thermal and
 * visual images, sends the collected data to configured API endpoints, and
 * enters deep sleep mode between operational cycles to conserve power.
 * It includes system status indication via a WS2812 LED and optional serial debugging output.
 */

// --- Core Arduino and System Includes ---
#include <Arduino.h>
#include <Wire.h>           // Required for I2C communication (BH1750, MLX90640)
#include "esp_heap_caps.h" // Required for memory checking/allocation functions (e.g., ps_malloc for PSRAM)
#include "esp_sleep.h"     // Required for ESP32 deep sleep functions

// --- Filesystem & JSON Includes ---
#include <FS.h>             // Filesystem base class
#include <LittleFS.h>       // LittleFS filesystem implementation for ESP32
#include <ArduinoJson.h>    // JSON parsing/serialization library

// --- Optional Debugging ---
// Uncomment the following line to enable Serial output for detailed debugging information.
#define ENABLE_DEBUG_SERIAL
// --------------------------

// --- Local Libraries (Project Specific Classes) ---
#include "DHT22Sensor.h"
#include "BH1750Sensor.h"
#include "MLX90640Sensor.h"
#include "OV2640Sensor.h"
#include "LEDStatus.h"
#include "EnvironmentDataJSON.h"
#include "MultipartDataSender.h"
#include "WiFiManager.h"
#include "ErrorLogger.h" 
#include "API.h"           

// --- Hardware Pin Definitions ---
#define I2C_SDA 47   ///< GPIO pin assigned for the I2C SDA (Data) line.
#define I2C_SCL 21   ///< GPIO pin assigned for the I2C SCL (Clock) line.
#define DHT_PIN 14   ///< GPIO pin connected to the DHT22 temperature/humidity sensor data line.

// --- General Configuration (Defaults & Constants) ---
#define SENSOR_READ_RETRIES 3       ///< Default number of attempts to read sensor data if an initial read fails.
#define WIFI_CONNECT_TIMEOUT_MS 20000 ///< Default timeout (in milliseconds) for establishing the initial WiFi connection in the loop.
#define CONFIG_FILENAME "/config.json" ///< Filename used for storing configuration settings on the LittleFS filesystem.

// --- Configuration Structure ---
/**
 * @brief Holds application configuration parameters loaded from LittleFS or default values.
 */
struct Config {
    String wifi_ssid = "DEFAULT_SSID";
    String wifi_pass = "";
    int deviceId = 0; 
    String activationCode = ""; 
    String apiBaseUrl = ""; 
    String apiActivatePath = "/api/device-api/activate";
    String apiAuthPath = "/api/device-api/auth";
    String apiRefreshTokenPath = "/api/device-api/refresh-token";
    String apiLogPath = "/api/device-api/log";
    String apiAmbientDataPath = "/api/device-api/ambient-data";
    String apiCaptureDataPath = "/api/device-api/capture-data";
    int sleep_sec = 60;
    bool debug_enabled = false; 
};
Config config; // Global instance holding the application's current configuration.
// -----------------------------

// --- I2C Bus Instance ---
// Using the default Wire object provided by the Arduino framework for I2C communication.
// Pins are configured in Wire.begin() using I2C_SDA and I2C_SCL defines.

// --- Global Object Instances ---
// Instantiating objects for each peripheral/utility class.
BH1750Sensor lightSensor(Wire, I2C_SDA, I2C_SCL); ///< Light sensor object using the default Wire I2C bus.
MLX90640Sensor thermalSensor(Wire);               ///< Thermal camera object using the default Wire I2C bus.
DHT22Sensor dhtSensor(DHT_PIN);                   ///< Temperature/Humidity sensor object connected to DHT_PIN.
OV2640Sensor camera;                              ///< Visual camera object (uses ESP-IDF driver).
LEDStatus led;                                    ///< Status LED management object.
WiFiManager wifiManager;                          ///< WiFi connection management object.

// --- Declare API object globally, initialize in setup ---
API* api_comm = nullptr;

// --- Function Prototypes ---

// Configuration and Initialization
/** @brief Loads configuration from LittleFS into the global 'config' struct. */
bool loadConfiguration(const char *filename);
/** @brief Initializes all hardware sensors sequentially. */
bool initializeSensors();

// Main Task Orchestration
/** @brief Reads environmental sensor data (light, temp, humidity) with retries. */
bool readEnvironmentData();
/** @brief Captures thermal and visual image data into allocated buffers. */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);

/** @brief Sends captured image data (thermal and visual) to the image API endpoint. */
// bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData);

// System Control
/** @brief Enters ESP32 deep sleep for a specified duration. */
void deepSleep(unsigned long seconds);
/** @brief Frees allocated memory buffers and deinitializes the camera. */
void cleanupResources(uint8_t* jpegImage, float* thermalData);
/** @brief Ensures WiFi is connected, blocking with a timeout if necessary. See WiFiManager for non-blocking handling. */
bool ensureWiFiConnected(unsigned long timeout);
/** @brief Performs a simple visual blink sequence using the status LED. */
void ledBlink();

// Setup Helper Prototypes
/** @brief Initializes the Serial port for debugging if ENABLE_DEBUG_SERIAL is defined. */
void initSerial();
/** @brief Initializes the status LED via the LEDStatus object. */
void initLED();
/** @brief Initializes and mounts the LittleFS filesystem, formatting if necessary. */
bool initFilesystem();
/** @brief Loads configuration and applies WiFi credentials to the WiFiManager. */
void loadConfigAndSetCredentials();
/** @brief Handles actions to take if sensor initialization fails (LED, sleep). */
void handleSensorInitFailure();

// Loop Helper Prototypes
/** @brief Handles the initial WiFi connection check in the main loop, sleeping on failure. */
bool handleWiFiConnection();
/** @brief Orchestrates reading and sending environmental data tasks. */
bool performEnvironmentTasks(Config& cfg, API& api_obj, LEDStatus& status_led);
/** @brief Orchestrates capturing and sending image data tasks. */
bool performImageTasks(Config& cfg, API& api_obj, LEDStatus& status_led, uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
/** @brief Performs final actions before entering deep sleep (LED status, FS unmount). */
void prepareForSleep(bool cycleStatusOK);
/** @brief Handles API authentication and activation, including token refresh. */
bool handleApiAuthenticationAndActivation(Config& cfg, API& api_obj, LEDStatus& status_led);

// Environment Task Helper Prototypes
/** @brief Reads the light sensor value with retries. */
bool readLightSensorWithRetry(float &lightLevel);
/** @brief Reads temperature and humidity from the DHT sensor with retries. */
bool readDHTSensorWithRetry(float &temperature, float &humidity);
/** @brief Sends the collected environmental data to the configured server endpoint. */
bool sendEnvironmentDataToServer(Config& cfg, API& api_obj, float lightLevel, float temperature, float humidity);

bool sendImageData(Config& cfg, API& api_obj, uint8_t* jpegImage, size_t jpegLength, float* thermalData);

// Image Capture Helper Prototypes
/** @brief Captures a thermal data frame and copies it into a newly allocated buffer. */
bool captureAndCopyThermalData(float** thermalDataBuffer);
/** @brief Captures a visual JPEG image into a buffer allocated by the camera driver. */
bool captureVisualJPEG(uint8_t** jpegImageBuffer, size_t& jpegLength);


// =========================================================================
// ===                           SETUP FUNCTION                          ===
// =========================================================================
/**
 * @brief Setup function, runs once on power-on or reset (including wake from deep sleep).
 * Initializes essential peripherals and systems in order: LED, Serial (optional),
 * LittleFS, loads configuration, sets WiFi credentials based on config, and initializes
 * all connected sensors.
 * If critical initialization steps (LittleFS, Sensors) fail, it halts or enters deep sleep
 * immediately to prevent undefined behavior in the main loop.
 */
void setup() {
    // Initialize LED first for early visual feedback during boot/setup.
    initLED();

    // Initialize Serial port for debugging output if enabled via #define.
    initSerial();

    // Initialize LittleFS filesystem. This is critical for loading configuration.
    // Halts execution indefinitely on catastrophic failure (cannot mount or format).
    if (!initFilesystem()) {
        // Specific error LED state might be set inside initFilesystem helper.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("CRITICAL: Halting execution due to filesystem initialization error.");
        #endif
        // Halt indefinitely to indicate critical failure. Requires manual reset/power cycle.
        while(1) { delay(1000); }
    }

    // Load configuration from the JSON file and set WiFi credentials in the WiFiManager.
    // Uses default values if loading fails, allowing operation with defaults.
    loadConfigAndSetCredentials();

    // --- Initialize API object AFTER config is loaded ---
    api_comm = new API(config.apiBaseUrl,
                       config.apiActivatePath,
                       config.apiAuthPath,
                       config.apiRefreshTokenPath);
    if (api_comm == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("CRITICAL: Failed to allocate memory for API object. Halting."));
        #endif
        led.setState(ERROR_AUTH); 
        while(1) { delay(1000); } 
    }

    // Initialize WiFi (sets mode, does not connect yet)
    wifiManager.begin();

    // Initialize all connected hardware sensors using the refactored helper function.
    if (!initializeSensors()) {
        // If any sensor fails initialization, handle the failure gracefully.
        // This typically involves setting an error LED and entering deep sleep.
        handleSensorInitFailure();
        // Note: Execution stops within handleSensorInitFailure if it enters deep sleep.
    }

    // Setup completed successfully if execution reaches here.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("--------------------------------------");
        Serial.println("Device setup completed successfully.");
        Serial.println("Initial LED state indicates OK.");
        Serial.println("--------------------------------------");
    #endif
    led.setState(ALL_OK); // Ensure final LED state reflects successful setup.
    delay(1000);          // Brief delay showing the OK status before entering the loop.
}


// =========================================================================
// ===                            MAIN LOOP                              ===
// =========================================================================
/**
 * @brief Main application loop function, runs repeatedly after setup completes.
 * Orchestrates the primary operational workflow cycle:
 * 1. Ensures WiFi connection is active (connects/retries if needed, sleeps on failure).
 * 2. Reads environmental sensor data and sends it to the server.
 * 3. Captures thermal and visual image data and sends it to the server.
 * 4. Cleans up resources allocated during the cycle (memory buffers, camera peripheral).
 * 5. Prepares for and enters deep sleep for the configured duration.
 */
void loop() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n======================================"));
        Serial.println(F("--- Starting New Operational Cycle ---"));
        // Print memory status at the start of a cycle for debugging
        Serial.printf("Free Heap: %u bytes, Min Free Heap: %u bytes\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
        #ifdef BOARD_HAS_PSRAM
            Serial.printf("Free PSRAM: %u bytes\n", ESP.getFreePsram());
        #endif
        Serial.println(F("======================================"));
    #endif

    // Perform a visual blink sequence to indicate the start of a new cycle.
    ledBlink();

    // Tracks the overall success of data operations in this cycle
    bool cycleStatusOK = true;
    // Initialize logUrl with the base API URL and log path.
    String logUrl = "";
    // Ensure api_comm is initialized before using it.
    if (api_comm != nullptr) { 
        logUrl = api_comm->getBaseApiUrl() + config.apiLogPath;
    } else {
        // This should not happen if setup completed. Halt or handle error.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("CRITICAL: api_comm is null in loop(). Halting."));
        #endif
        led.setState(ERROR_DATA); // Generic system error
        while(1) delay(1000);
    }

    // --- 1. Handle WiFi Connection ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 1: Checking WiFi Connection..."));
    #endif
    wifiManager.handleWiFi(); // Non-blocking handler
    if (!ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("WiFi connection failed. Entering deep sleep."));
        #endif
        led.setState(ERROR_WIFI); // ensureWiFiConnected should set this, but good to be sure
        prepareForSleep(false);   // Mark cycle as not OK
        deepSleep(config.sleep_sec); // Use fallback sleep duration
        return; // Exit loop
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("WiFi Connection OK."));
    #endif

    // --- 2. Handle API Activation and Authentication ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 2: Handling API Activation & Authentication..."));
    #endif
    if (api_comm == nullptr) { // Should not happen if setup is correct
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("CRITICAL: api_comm object is null in loop. Halting."));
        #endif
        led.setState(ERROR_DATA); // Generic system error
        while(1) delay(1000); // Halt
    }

    bool apiReady = handleApiAuthenticationAndActivation(config, *api_comm, led);

    if (!apiReady) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("API not ready (activation/auth failed). Entering deep sleep."));
        #endif
        // LED state should have been set by handleApiAuthenticationAndActivation
        prepareForSleep(false);
        deepSleep(api_comm->getDataCollectionTimeMinutes() > 0 ? api_comm->getDataCollectionTimeMinutes() * 60 : config.sleep_sec);
        return; // Exit loop
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("API Ready. AccessToken available. Proceeding with data tasks."));
    #endif

    // --- 3. Perform Environmental Data Tasks ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 3: Performing Environment Data Tasks..."));
    #endif
    if (!performEnvironmentTasks(config, *api_comm, led)) {
        cycleStatusOK = false; // Mark cycle as having an error
        // Error logging and LED state should be handled within performEnvironmentTasks or its sub-functions
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Environment data tasks failed."));
        #endif
    }

    // --- 4. Perform Image Data Tasks ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 4: Performing Image Data Tasks..."));
    #endif
    uint8_t* jpegImage = nullptr;
    size_t jpegLength = 0;
    float* thermalData = nullptr; // MLX90640 data

    // Pass config and api_comm to performImageTasks
    if (!performImageTasks(config, *api_comm, led, &jpegImage, jpegLength, &thermalData)) {
        cycleStatusOK = false; // Mark cycle as having an error
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Image data tasks failed."));
        #endif
    }
    // Note: jpegImage and thermalData are allocated by captureImages (called within performImageTasks)
    // and need to be freed by cleanupResources.

    // --- 5. Cleanup Allocated Resources ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 5: Cleaning up resources..."));
    #endif
    cleanupResources(jpegImage, thermalData);

    // --- 6. Log Cycle Status ---
    if (!logUrl.isEmpty() && !api_comm->getAccessToken().isEmpty()) {
        if (cycleStatusOK) {
            ErrorLogger::sendLog(logUrl, api_comm->getAccessToken(), LOG_TYPE_INFO, "Operational cycle completed successfully.");
        } else {
            ErrorLogger::sendLog(logUrl, api_comm->getAccessToken(), LOG_TYPE_WARNING, "Operational cycle completed with errors.");
        }
    }

    // --- 7. Prepare for Deep Sleep ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Step 7: Preparing for Deep Sleep..."));
    #endif
    prepareForSleep(cycleStatusOK);

    // --- 8. Enter Deep Sleep ---
    unsigned long sleepDurationSec = api_comm->getDataCollectionTimeMinutes() > 0 ?
                                     api_comm->getDataCollectionTimeMinutes() * 60 :
                                     config.sleep_sec;
    deepSleep(sleepDurationSec);
    // --- Execution stops here until wake up ---

    // --- 5. Cleanup Allocated Resources ---
    cleanupResources(jpegImage, thermalData); // Frees jpegImage and thermalData if allocated

    // --- 6. Send Final Cycle Log ---
    // (Moved here to ensure it's sent after all data operations)
    if (cycleStatusOK) {
        ErrorLogger::sendLog(logUrl, api_comm->getAccessToken(), LOG_TYPE_INFO, "Operational cycle completed successfully.");
    } else {
        ErrorLogger::sendLog(logUrl, api_comm->getAccessToken(), LOG_TYPE_WARNING, "Operational cycle completed with errors.");
    }

    // --- 7. Prepare for Deep Sleep ---
    prepareForSleep(cycleStatusOK);

    // --- 8. Enter Deep Sleep ---
    // sleepDurationSec already declared above, just update if needed
    sleepDurationSec = config.sleep_sec; // Default fallback
    if (api_comm->getDataCollectionTimeMinutes() > 0) {
        sleepDurationSec = api_comm->getDataCollectionTimeMinutes() * 60UL;
    }
     #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[Loop] Determined sleep duration: %lu seconds.\n", sleepDurationSec);
    #endif
    deepSleep(sleepDurationSec);
    // --- Execution stops here until the device wakes up ---
}


// =========================================================================
// ===                        HELPER FUNCTIONS                           ===
// =========================================================================

/**
 * @brief Handles the device activation and authentication process with the API.
 * This function checks if the device is activated. If not, it attempts to activate it.
 * If activated (or after successful activation), it checks backend status and authentication.
 * Logs relevant events and errors. Sets LED status accordingly.
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the global API object.
 * @param status_led A reference to the global LEDStatus object.
 * @return true if the device is activated, authenticated, and ready for data operations.
 * @return false if activation or authentication fails critically.
 */
bool handleApiAuthenticationAndActivation(Config& cfg, API& api_obj, LEDStatus& status_led) {
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    if (!api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Handler] Device not activated. Attempting activation..."));
        #endif
        status_led.setState(CONNECTING_WIFI); // Or a specific "ACTIVATING" state if you add one

        int activationHttpCode = api_obj.performActivation(String(cfg.deviceId), cfg.activationCode);

        if (activationHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[API_Handler] Activation successful."));
            #endif
            // Send INFO log for successful activation.
            // Check for token availability before trying to send an authenticated log.
            String currentTokenForLog = api_obj.getAccessToken(); 
            if (!currentTokenForLog.isEmpty()) {
                ErrorLogger::sendLog(logUrl, currentTokenForLog, LOG_TYPE_INFO, "Device activated successfully. DeviceID: " + String(cfg.deviceId));
            } else {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[API_Handler] Activation HTTP 200, but no access token available for logging."));
                #endif
                // This case might indicate an issue in API::performActivation's token parsing/storage on HTTP 200
            }
            if (!api_obj.getAccessToken().isEmpty()) {
                ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, "Device activated successfully. DeviceID: " + String(cfg.deviceId));
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[API_Handler] Activation reported success, but no access token found for logging."));
                 #endif
                 // This case might indicate an issue in API::performActivation's token parsing/storage on HTTP 200
            }
            status_led.setState(ALL_OK); // Indicate success briefly
            delay(500);
            // Now that it's activated, proceed to checkBackendAndAuth to ensure tokens are fine
            // and dataCollectionTime is potentially updated.
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_Handler] Activation failed. HTTP Code: %d\n", activationHttpCode);
            #endif
            status_led.setState(ERROR_AUTH); // Persistent auth error
            // Try to send an ERROR log. Since activation failed, we might not have a token.
            // The ErrorLogger::sendLog function should ideally handle an empty accessToken gracefully (e.g., not send or try unauthenticated).
            // For now, we attempt, but it might fail if the log endpoint requires auth.
            ErrorLogger::sendLog(logUrl, "", LOG_TYPE_ERROR, "Device activation failed. HTTP Code: " + String(activationHttpCode) + ", DeviceID: " + String(cfg.deviceId));
            return false; // Critical failure, cannot proceed.
        }
    }

    // If we reach here, device is (or was just) activated.
    // Perform backend/auth check.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[API_Handler] Device activated. Performing backend and auth check..."));
    #endif
    status_led.setState(CONNECTING_WIFI); // Indicates network activity

    int authHttpCode = api_obj.checkBackendAndAuth();

    if (authHttpCode == 200) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Handler] Backend & Auth check successful."));
        #endif
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, "Backend and Auth check successful.");
        status_led.setState(ALL_OK);
        return true; // Ready to operate
    } else {
        // checkBackendAndAuth internally handles setting isActivated=false on critical refresh failure.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_Handler] Backend & Auth check failed. HTTP Code: %d\n", authHttpCode);
        #endif
        status_led.setState(ERROR_AUTH);

        String errorMessage = "Backend/Auth check failed. HTTP Code: " + String(authHttpCode);
        if (!api_obj.isActivated()) { // Check if it was deactivated due to refresh failure
            errorMessage += ". Device has been deactivated.";
        }
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, errorMessage);
        // Even if getAccessToken() is empty due to deactivation, sendLog should handle it.

        return false; // Critical failure or backend not ready.
    }
}

/**
 * @brief Loads configuration from a JSON file on LittleFS into the global 'config' struct.
 * Reads the specified file, parses its JSON content, and updates the members
 * of the global `config` object. If the file doesn't exist, cannot be opened,
 * or contains invalid JSON, default values already present in the `config` struct
 * will be used.
 * @param filename The full path to the configuration file on the LittleFS filesystem (e.g., "/config.json").
 * @return `true` if the configuration file was successfully opened, parsed, and values were extracted.
 * `false` otherwise (file not found, parse error). Defaults are used in case of `false`.
 */
bool loadConfiguration(const char *filename) {
    // Attempt to open the configuration file for reading.
    File configFile = LittleFS.open(filename, "r");
    if (!configFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Warning: Failed to open config file: " + String(filename) + ". Using defaults.");
        #endif
        return false; // Indicate failure to open file (defaults will be used).
    }

    // Allocate a JsonDocument. Using dynamic allocation on ESP32 is generally fine.
    // Adjust size calculation/consider StaticJsonDocument if memory constraints are critical.
    // See ArduinoJson documentation for capacity calculation.
    JsonDocument doc; // Using dynamic allocation by default

    // Parse the JSON content directly from the file stream.
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close(); // Close the file as soon as possible.

    // Check for JSON parsing errors.
    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("Warning: Failed to parse config file JSON: ");
            Serial.println(error.c_str());
            Serial.println("Using default configuration values.");
        #endif
        return false; // Indicate parsing failure (defaults will be used).
    }

    // Extract configuration values from the parsed JSON document.
    // The `|` operator provides a default value if the key is missing or the value is null.
    config.wifi_ssid = doc["wifi_ssid"] | config.wifi_ssid;
    config.wifi_pass = doc["wifi_pass"] | config.wifi_pass;

    config.deviceId = doc["FIRMWARE_DEVICE_ID"] | config.deviceId;
    config.activationCode = doc["FIRMWARE_ACTIVATION_CODE"] | config.activationCode;

    config.apiBaseUrl = doc["api_base_url"] | config.apiBaseUrl;
    config.apiActivatePath = doc["api_activate_path"] | config.apiActivatePath;
    config.apiAuthPath = doc["api_auth_path"] | config.apiAuthPath;
    config.apiRefreshTokenPath = doc["api_refresh_token_path"] | config.apiRefreshTokenPath;
    config.apiLogPath = doc["api_log_path"] | config.apiLogPath;
    config.apiAmbientDataPath = doc["api_ambient_data_path"] | config.apiAmbientDataPath;
    config.apiCaptureDataPath = doc["api_capture_data_path"] | config.apiCaptureDataPath;

    config.sleep_sec = doc["sleep_sec"] | config.sleep_sec;
    config.debug_enabled = doc["debug_enabled"] | config.debug_enabled; 

#ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Configuration loaded successfully from file:");
    Serial.println("  WiFi SSID: " + config.wifi_ssid);
    // Serial.println("  WiFi Pass: [REDACTED]"); // Keep password redacted
    Serial.println("  Device ID: " + String(config.deviceId));
    // Serial.println("  Activation Code: [REDACTED]"); // Activation code should also be redacted
    Serial.println("  API Base URL: " + config.apiBaseUrl);
    Serial.println("  API Activate Path: " + config.apiActivatePath);
    Serial.println("  API Auth Path: " + config.apiAuthPath);
    Serial.println("  API Refresh Token Path: " + config.apiRefreshTokenPath);
    Serial.println("  API Log Path: " + config.apiLogPath);
    Serial.println("  API Ambient Data Path: " + config.apiAmbientDataPath);
    Serial.println("  API Capture Data Path: " + config.apiCaptureDataPath);
    Serial.println("  Sleep Seconds: " + String(config.sleep_sec));
    Serial.println("  Debug Enabled: " + String(config.debug_enabled ? "true" : "false"));
#endif
    return true; // Indicate successful loading from file.
}

/**
 * @brief Ensures WiFi is connected, blocking with a timeout if necessary.
 * Checks the current connection status via WiFiManager. If not connected,
 * it initiates a connection attempt and waits up to the specified timeout
 * for the connection to establish (status becomes CONNECTED). Uses the
 * WiFiManager's non-blocking handler internally during the wait loop.
 * Sets LED status appropriately.
 * @param timeout The maximum time in milliseconds to wait for a connection.
 * @return `true` if the device is connected to WiFi (either initially or after connecting within the timeout).
 * `false` if the connection attempt fails or times out.
 */
bool ensureWiFiConnected(unsigned long timeout) {
    // Check if already connected - the common case after the first cycle.
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
        return true; // Already connected, nothing more to do.
    }

    // If not connected and not already trying to connect, start the process.
    if (wifiManager.getConnectionStatus() != WiFiManager::CONNECTING) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("WiFi is not connected. Initiating connection attempt...");
        #endif
        wifiManager.connectToWiFi(); // Start the non-blocking connection attempt.
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("WiFi connection already in progress...");
         #endif
    }

    // Set LED to indicate connecting status while we wait.
    led.setState(CONNECTING_WIFI);
    unsigned long startTime = millis(); // Record start time for the timeout check.

    // Blocking wait loop: Check status repeatedly until connected, failed, or timed out.
    while (millis() - startTime < timeout) {
        // Allow the WiFiManager's internal state machine to run (handles events, retries).
        wifiManager.handleWiFi();

        // Check if connection succeeded.
        if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("WiFi connection established successfully within timeout.");
            #endif
            return true; // Connection successful.
        }

        // Check if the manager has determined a permanent failure (max retries hit).
        if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("WiFi connection failed permanently (max retries reached by WiFiManager).");
            #endif
            break; // Exit the wait loop, connection failed.
        }

        // Small delay to prevent busy-waiting and allow other tasks (if any).
        delay(100);
    }

    // If the loop finishes without returning true, it means connection failed or timed out.
    #ifdef ENABLE_DEBUG_SERIAL
        if (wifiManager.getConnectionStatus() != WiFiManager::CONNECTION_FAILED) {
             Serial.println("WiFi connection attempt timed out after " + String(timeout) + " ms.");
        }
    #endif
    led.setState(ERROR_WIFI); // Indicate WiFi error on the LED.
    return false; // Connection failed within the specified timeout.
}

/**
 * @brief Performs a simple visual blink sequence using the status LED.
 * Used to indicate activity or the start/end of a cycle.
 */
void ledBlink() {
    // Sequence: OK -> OFF -> OK -> OFF -> OK -> OFF
    led.setState(ALL_OK); delay(350);
    led.setState(OFF); delay(150);
    led.setState(ALL_OK); delay(350);
    led.setState(OFF); delay(150);
    led.setState(ALL_OK); delay(350);
    led.setState(OFF); delay(150);
}

/**
 * @brief Initializes all connected hardware sensors sequentially.
 * Initializes DHT22, I2C bus (for BH1750 & MLX90640), BH1750, MLX90640, and OV2640.
 * Includes necessary delays between initializations, especially for I2C sensors
 * and the MLX90640 stabilization period. Sets error LED and returns false
 * immediately if any sensor initialization fails.
 * @return `true` if all sensors were initialized successfully. `false` if any sensor failed.
 */
bool initializeSensors() {
    // --- Initialize DHT22 Sensor ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing DHT22 sensor...");
    #endif
    // DHT library begin() usually doesn't return a status. Assume OK for now.
    dhtSensor.begin();
    delay(100); // Short delay after DHT init

    // --- Initialize I2C Bus (Wire) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing I2C Bus (Pins SDA: " + String(I2C_SDA) + ", SCL: " + String(I2C_SCL) + ", Freq: 100kHz)...");
    #endif
    Wire.begin(I2C_SDA, I2C_SCL); // Initialize I2C with specified pins
    Wire.setClock(100000);        // Set I2C clock speed (100kHz is common, check sensor datasheets)
    delay(100);                   // Delay after I2C init

    // --- Initialize BH1750 Light Sensor (via I2C) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing BH1750 (Light Sensor) via I2C...");
    #endif
    if (!lightSensor.begin()) { // begin() returns true on success, false on failure
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("!!! BH1750 Light Sensor Initialization FAILED !!!");
        #endif
        led.setState(ERROR_SENSOR); // Set LED to sensor error state
        delay(3000);                // Show error state briefly
        return false;               // Return failure status
    }
    delay(100); // Short delay after light sensor init

    // --- Initialize MLX90640 Thermal Sensor (via I2C) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing MLX90640 (Thermal Sensor) via I2C...");
    #endif
    if (!thermalSensor.begin()) { // begin() returns true on success, false on failure
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("!!! MLX90640 Thermal Sensor Initialization FAILED !!!");
        #endif
        led.setState(ERROR_SENSOR); // Set LED to sensor error state
        delay(3000);                // Show error state briefly
        return false;               // Return failure status
    }
    // IMPORTANT: Delay required after MLX90640 begin() before first readFrame()
    // This allows the sensor to stabilize and acquire its first valid readings.
    // The duration depends on the configured refresh rate (e.g., ~2s for 0.5Hz).
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Waiting for MLX90640 measurement stabilization (~2 seconds)...");
    #endif
    delay(2000); // Adjust delay based on MLX90640 refresh rate setting in MLX90640Sensor::begin()

    // --- Initialize OV2640 Camera Sensor ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing OV2640 (Visual Camera)...");
    #endif
    if (!camera.begin()) { // begin() returns true on success, false on failure
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("!!! OV2640 Camera Initialization FAILED !!!");
        #endif
        led.setState(ERROR_SENSOR); // Set LED to sensor error state
        delay(3000);                // Show error state briefly
        // Note: Consider de-initializing already initialized sensors here if needed?
        return false;               // Return failure status
    }
    delay(500); // Delay after camera init

    // If execution reaches here, all sensors initialized successfully.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("All sensors initialized successfully.");
    #endif
    return true; // Return success status
}

/**
 * @brief Orchestrates reading environmental data and sending it to the server.
 * This function calls helper functions that handle sensor reading with retries
 * (`readLightSensorWithRetry`, `readDHTSensorWithRetry`) and data sending
 * (`sendEnvironmentDataToServer`). It manages the LED status during the process
 * and returns an overall success/failure status for the entire environmental task.
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the global API object.
 * @return `true` if sensor data was successfully read AND successfully sent to the server.
 * `false` if either the sensor reading or the data sending failed.
 */
bool readEnvironmentData(Config& cfg, API& api_obj) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- Reading Environment Sensors & Sending Data ---"));
    #endif

    float lightLevel = -1.0f;
    float temperature = NAN;
    float humidity = NAN;

    led.setState(TAKING_DATA);
    // delay(1000); // Optional

    bool lightOK = readLightSensorWithRetry(lightLevel);
    bool dhtOK = readDHTSensorWithRetry(temperature, humidity);

    if (!lightOK || !dhtOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Error: Failed to read one or more environment sensors after retries."));
        #endif
        led.setState(ERROR_SENSOR); // Changed from ERROR_DATA to be more specific
        delay(1000);
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, "Failed to read environment sensors.");
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Environment sensors read successfully. Preparing to send..."));
        // Serial.printf("  Read Values: Light=%.2f lx, Temperature=%.2f C, Humidity=%.2f %%\n", lightLevel, temperature, humidity);
    #endif

    // Pass cfg and api_obj to sendEnvironmentDataToServer
    if (!sendEnvironmentDataToServer(cfg, api_obj, lightLevel, temperature, humidity)) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Error: Failed to send environment data to the server (handled in sendEnvironmentDataToServer)."));
        #endif
        // LED state and logging are handled within sendEnvironmentDataToServer
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Environment data sent successfully to the server."));
    #endif
    return true;
}

/**
 * @brief Orchestrates capturing both thermal and visual images.
 * Calls helper functions (`captureAndCopyThermalData`, `captureVisualJPEG`)
 * that handle the actual capture and memory allocation. This function manages
 * the output pointers and ensures resources are handled correctly if one capture fails.
 * Sets LED state to indicate data capture error on failure.
 * @param[out] jpegImage Pointer to a `uint8_t*`. On success, this will point to the buffer containing the allocated JPEG data. Set to `nullptr` on failure.
 * @param[out] jpegLength Reference to a `size_t`. On success, this will contain the size (in bytes) of the JPEG data. Set to 0 on failure.
 * @param[out] thermalData Pointer to a `float*`. On success, this will point to the buffer containing the allocated thermal data (768 floats). Set to `nullptr` on failure.
 * @return `true` if BOTH the thermal data capture/copy AND the visual JPEG capture were successful.
 * `false` if either capture failed. Resources allocated by a successful first capture will be freed if the second fails.
 */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("--- Capturing Thermal and Visual Images ---");
    #endif
    // Ensure output pointers are initialized to null/zero before attempting capture.
    *jpegImage = nullptr;
    *thermalData = nullptr;
    jpegLength = 0;

    // Set LED to indicate data acquisition phase.
    led.setState(TAKING_DATA);
    delay(1000); // Optional delay before capture.

    // --- 1. Capture Thermal Data ---
    // This helper reads the frame and *allocates* memory for the thermal data copy.
    if (!captureAndCopyThermalData(thermalData)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Error: Failed to capture or copy thermal data.");
        #endif
        // If thermal data capture fails, set error LED and return immediately.
        // No visual capture attempted, no buffers need freeing yet.
        led.setState(ERROR_DATA);
        delay(3000); // Show error state briefly.
        return false;
    }
    // If thermal capture succeeded, *thermalData now points to allocated memory.

    // --- 2. Capture Visual JPEG Data ---
    // This helper captures the image and *allocates* memory for the JPEG data.
    if (!captureVisualJPEG(jpegImage, jpegLength)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Error: Failed to capture visual JPEG image.");
        #endif
        // If visual capture fails, set error LED.
        led.setState(ERROR_DATA);
        delay(3000); // Show error state briefly.

        // *** CRITICAL CLEANUP ***: Since thermal capture succeeded but visual failed,
        // we MUST free the buffer allocated for thermal data to prevent a memory leak.
        if (*thermalData != nullptr) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("Cleaning up thermal data buffer due to subsequent JPEG capture failure.");
            #endif
            free(*thermalData);       // Free the previously allocated thermal buffer.
            *thermalData = nullptr;   // Avoid dangling pointer.
        }
        return false; // Return failure status for the overall capture task.
    }
    // If execution reaches here, *thermalData and *jpegImage point to allocated buffers.

    // If both captures were successful.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Thermal and visual images captured successfully.");
    #endif
    // Optionally set LED back to OK briefly.
    // led.setState(ALL_OK); delay(500);
    return true; // Indicate overall success for the image capture task.
}

/**
 * @brief Sends the captured thermal and visual image data to the server via multipart POST.
 * Uses the `MultipartDataSender` utility class to perform the HTTP request.
 * Sets LED status according to the outcome (SENDING_DATA during attempt, ERROR_SEND on failure).
 * @param jpegImage Pointer to the buffer containing the JPEG image data.
 * @param jpegLength Size of the JPEG image data in bytes.
 * @param thermalData Pointer to the buffer containing the thermal data (768 floats).
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the global API object.
 * @return `true` if the data was sent successfully (HTTP 2xx response received).
 * `false` otherwise (HTTP request failed or server returned error).
 */
bool sendImageData(Config& cfg, API& api_obj, uint8_t* jpegImage, size_t jpegLength, float* thermalData) { 
    if (!jpegImage || jpegLength == 0 || !thermalData) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Error: Invalid data provided to sendImageData."));
        #endif
        led.setState(ERROR_DATA);
        delay(1000);
        return false;
    }

    led.setState(SENDING_DATA);
    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiCaptureDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("  Preparing to send thermal and image data via HTTP POST (multipart)..."));
        Serial.println("  Target URL: " + fullUrl);
    #endif
    // delay(1000); // Optional

    int httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);

    if (httpCode == 200 || httpCode == 204) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("  Image and thermal data sent successfully."));
        #endif
        return true;
    } else if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("  Capture data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_WARNING, "Capture data send returned 401. Attempting token refresh.");

        int refreshHttpCode = api_obj.performTokenRefresh();
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("  Token refresh successful. Re-trying capture data send..."));
            #endif
             ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, "Token refreshed successfully after capture data 401.");
            token = api_obj.getAccessToken(); // Get new token
            httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("  Capture data sent successfully on retry."));
                #endif
                return true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("  Capture data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Capture data send failed on retry after refresh. HTTP: " + String(httpCode));
            }
        } else {
             #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("  Token refresh failed after 401. HTTP Code: %d\n", refreshHttpCode);
            #endif
            ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Token refresh failed after capture data 401. Refresh HTTP: " + String(refreshHttpCode));
        }
    } else { // Other HTTP errors or client errors
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("  Error sending image/thermal data. HTTP Code: %d\n", httpCode);
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Failed to send capture data. HTTP Code: " + String(httpCode));
    }

    led.setState(ERROR_SEND);
    delay(1000);
    return false;
}

/**
 * @brief Cleans up resources allocated during the operational cycle.
 * Specifically, frees the memory buffers allocated for the JPEG image
 * and the thermal data copy using `free()`. Also deinitializes the
 * camera peripheral by calling `camera.end()` to release hardware resources.
 * This should be called before entering deep sleep.
 * @param jpegImage Pointer to the allocated JPEG image buffer (or nullptr if allocation failed/not captured).
 * @param thermalData Pointer to the allocated thermal data buffer (or nullptr if allocation failed/not captured).
 */
void cleanupResources(uint8_t* jpegImage, float* thermalData) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("--- Cleaning Up Resources ---");
    #endif

    // Free the JPEG image buffer if it was allocated.
    if (jpegImage != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Freeing JPEG image buffer...");
        #endif
        free(jpegImage); // Use free() as memory was likely allocated by ps_malloc/malloc
        // No need to set jpegImage = nullptr here as it's a local copy of the pointer in loop()
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            // Serial.println("No JPEG image buffer to free.");
         #endif
    }

    // Free the thermal data buffer if it was allocated.
    if (thermalData != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Freeing thermal data buffer...");
        #endif
        free(thermalData); // Use free() as memory was likely allocated by ps_malloc/malloc
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            // Serial.println("No thermal data buffer to free.");
         #endif
    }

    // Deinitialize the camera peripheral to release hardware resources (I2S, DMA etc.)
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Deinitializing camera peripheral (calling camera.end())...");
    #endif
    camera.end(); // Calls esp_camera_deinit() internally
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Camera deinitialized.");
    #endif

    delay(50); // Short delay after cleanup if needed.
}

/**
 * @brief Enters ESP32 deep sleep mode for a specified number of seconds.
 * Configures the ESP32 to wake up using the timer peripheral after the
 * given duration. Turns off the status LED before sleeping. Execution
 * effectively stops here until the device wakes up (which causes a reset).
 * @param seconds The duration (unsigned long) to sleep in seconds.
 */
void deepSleep(unsigned long seconds) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("Entering deep sleep for %lu seconds...\n", seconds);
        Serial.println("--------------------------------------");
        Serial.flush(); // Ensure all serial messages are sent before sleeping
        delay(100);     // Short delay for serial flush
    #endif
    led.turnOffAll(); // Turn off LED before sleeping.

    // Configure the timer wakeup source.
    // The duration is specified in microseconds (seconds * 1,000,000).
    // Use ULL suffix for large constants to avoid overflow.
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);

    // Enter deep sleep mode. The device will reset upon waking up.
    esp_deep_sleep_start();

    // --- Code execution will NOT reach here ---
}


// =========================================================================
// ===                  SETUP HELPER IMPLEMENTATIONS                     ===
// =========================================================================

/**
 * @brief Initializes the Serial port for debugging output if enabled.
 * Starts Serial communication at 115200 baud and prints a boot/wake message
 * including the device's unique chip ID (MAC address based).
 */
void initSerial() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.begin(115200); // Start Serial communication
        // while (!Serial); // Optional: Wait for a serial monitor connection - remove for unattended operation.
        delay(1000); // Give Serial time to initialize, especially after reset.
        uint64_t chipid = ESP.getEfuseMac(); // Get unique chip ID (MAC address)
        Serial.printf("\n--- Device Booting / Waking Up (Chip ID: %04X%08X) ---\n",
                      (uint16_t)(chipid >> 32), // High part of MAC
                      (uint32_t)chipid);        // Low part of MAC
        Serial.println("Debug Serial Enabled (Rate: 115200)");
    #endif
}

/**
 * @brief Initializes the status LED.
 * Calls the begin() method of the global LEDStatus object and sets
 * an initial state (e.g., ALL_OK) to indicate the setup process has started.
 */
void initLED() {
    led.begin();   // Initialize the NeoPixel library via the wrapper
    led.setState(ALL_OK); // Set an initial "OK" state during boot/setup
}

/**
 * @brief Initializes the LittleFS filesystem.
 * Attempts to mount the filesystem. If mounting fails, it attempts to
 * format the filesystem and then mount it again. If formatting also fails,
 * it indicates a critical error (sets error LED) and returns false.
 * @return `true` if the filesystem is successfully mounted (either initially or after formatting).
 * `false` if mounting and formatting both fail (critical error).
 */
bool initFilesystem() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Initializing LittleFS filesystem...");
    #endif
    // Attempt to mount LittleFS. false = do not format if mount fails on the first try.
    if (!LittleFS.begin(false)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Warning: Initial LittleFS mount failed! Attempting to format...");
        #endif
        // If initial mount failed, try again with format_if_failed = true.
        // This will format the filesystem ONLY if it cannot be mounted.
        delay(1000); // Give it a moment before formatting
        if (!LittleFS.begin(true)) { // true = format if mount still fails
            // If begin(true) also fails, formatting failed - this is a critical error.
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("CRITICAL ERROR: Formatting LittleFS failed! Check hardware/partition scheme.");
            #endif
            led.setState(ERROR_DATA); // Use a relevant error state (e.g., data/storage error)
            delay(5000);              // Show error state clearly
            return false;             // Return critical failure status.
        } else {
            // Filesystem was successfully formatted and mounted.
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("LittleFS formatted successfully. Filesystem is now empty.");
                Serial.println("Configuration file will be missing; defaults will be used.");
            #endif
            // Proceed, loadConfiguration will handle missing file using defaults.
        }
    }
    // If execution reaches here, LittleFS is mounted (either initially or after format).
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("LittleFS mounted successfully.");
        // Optional: List directory contents or show FS info
        // File root = LittleFS.open("/"); File file = root.openNextFile(); ... etc.
        // Serial.printf(" FS Info: Total bytes: %ld, Used bytes: %ld\n", LittleFS.totalBytes(), LittleFS.usedBytes());
    #endif
    return true; // Filesystem is ready.
}

/**
 * @brief Loads the application configuration and sets WiFi credentials.
 * Calls `loadConfiguration()` to populate the global `config` struct from
 * the file specified by `CONFIG_FILENAME`. If loading fails, default values
 * in the `config` struct are used. Then, it passes the loaded (or default)
 * SSID and password to the `wifiManager` instance.
 */
void loadConfigAndSetCredentials() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Loading configuration from file: " + String(CONFIG_FILENAME));
    #endif
    // Attempt to load config. If it fails, the global 'config' struct retains defaults.
    if (!loadConfiguration(CONFIG_FILENAME)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Warning: loadConfiguration failed or file not found. Proceeding with default config values.");
        #endif
    }

    // Pass the loaded or default credentials to the WiFi Manager instance.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Setting WiFi credentials in WiFiManager for SSID: " + config.wifi_ssid);
    #endif
    wifiManager.setCredentials(config.wifi_ssid, config.wifi_pass);
}

/**
 * @brief Handles the failure scenario during sensor initialization in setup().
 * Logs the failure (if debug enabled), ensures the error LED state is set
 * (should be set by the failing sensor init function), unmounts the filesystem
 * cleanly, and then enters deep sleep using the configured sleep duration.
 * This function does not return, as `deepSleep()` halts execution.
 */
void handleSensorInitFailure() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("CRITICAL ERROR: Sensor initialization failed during setup.");
        Serial.println("Preparing to enter deep sleep to save power / allow potential recovery on wake.");
    #endif
    // The specific sensor init function should have already set led.setState(ERROR_SENSOR).
    // We might add a delay here to ensure the LED state is visible.
    delay(3000);

    // Unmount the filesystem cleanly before sleeping.
    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Unmounted LittleFS.");
    #endif
    delay(500); // Short delay before sleeping.

    // Enter deep sleep using the configured sleep duration.
    deepSleep(config.sleep_sec);
    // --- Execution stops here ---
}


// =========================================================================
// ===                   LOOP HELPER IMPLEMENTATIONS                     ===
// =========================================================================

/**
 * @brief Manages the WiFi connection check at the beginning of each loop cycle.
 * Calls `ensureWiFiConnected` which blocks until connected or timeout/failure.
 * If connection fails, it initiates the deep sleep sequence immediately, including
 * resource cleanup, as proceeding without WiFi is not viable for this application.
 * @return `true` if WiFi connection is verified or established successfully.
 * `false` if WiFi connection fails (in which case deep sleep is initiated and this function technically doesn't return normally).
 */
bool handleWiFiConnection() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("--- Checking WiFi Connection ---");
    #endif
    // ensureWiFiConnected blocks until connected, failed, or timeout.
    if (!ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS)) {
        // If connection failed after blocking wait.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Error: WiFi connection could not be established within timeout in main loop.");
            Serial.println("Entering deep sleep cycle immediately.");
        #endif
        // The LED should already be set to ERROR_WIFI by ensureWiFiConnected.
        delay(3000); // Ensure the error LED is visible.

        // --- Critical: Need to cleanup resources before sleeping ---
        // Even though image buffers weren't allocated yet in this cycle,
        // the camera might have been initialized (if setup succeeded).
        // Passing nullptrs for buffers is safe for cleanupResources.
        cleanupResources(nullptr, nullptr);

        // Unmount filesystem before sleeping.
        LittleFS.end();
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Unmounted LittleFS.");
        #endif

        // Enter deep sleep.
        deepSleep(config.sleep_sec);
        // --- Execution stops here ---
        return false; // Technically unreachable, but indicates failure path.
    }

    // If ensureWiFiConnected returned true.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFi Connection OK.");
        // Optional: Check actual internet connectivity if needed here using wifiManager.checkInternetConnection()
        // if (!wifiManager.checkInternetConnection()) { ... handle lack of internet ... }
    #endif
    return true; // Indicate WiFi is connected and ready.
}

/**
 * @brief Performs the environmental data task sequence: reading sensors and sending data.
 * Calls the `readEnvironmentData` function which handles the sub-steps (reading
 * with retries, sending data) and manages LED status for this task.
 * Logs the overall success or failure of the task.
 * @param cfg A reference to the global Config structure.
 * @param api_obj A reference to the global API object.
 * @return `true` if the environmental data task (read & send) completed successfully.
 * `false` otherwise.
 */
bool performEnvironmentTasks(Config& cfg, API& api_obj, LEDStatus& status_led) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n--- Performing Environment Data Tasks (Wrapper) ---"));
    #endif

    // Pass cfg and api_obj to readEnvironmentData
    if (!readEnvironmentData(cfg, api_obj)) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Result: Environment Data Task FAILED."));
        #endif
        // Error LED and logging should be set within readEnvironmentData or sendEnvironmentDataToServer
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Result: Environment Data Task SUCCEEDED."));
    #endif
    return true;
}

/**
 * @brief Captures images from the camera and thermal sensor, allocating buffers
 * for the captured data. Calls `captureVisualJPEG()` and `captureAndCopyThermalData()`.
 * Allocated buffers are passed back to the caller via output parameters.
 * @param[out] jpegImage Pointer to a `uint8_t*` variable in the caller. On success,
 * this will be updated to point to the allocated JPEG buffer. Set to `nullptr` on failure.
 * @param[out] jpegLength Reference to a `size_t` variable in the caller. On success,
 * this will be updated with the size (in bytes) of the captured JPEG image. Set to 0 on failure.
 * @param[out] thermalData Pointer to a `float*` variable in the caller. On success,
 * this will be updated to point to the allocated thermal data buffer. Set to `nullptr` on failure.
 * @return `true` if both image capture and buffer allocation were successful.
 * `false` otherwise (any capture or allocation failed).
 */
bool performImageTasks(Config& cfg, API& api_obj, LEDStatus& status_led, uint8_t** jpegImage, size_t& jpegLength, float** thermalData) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n--- Performing Image Data Tasks (Wrapper) ---"));
    #endif

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Task Step: Capture Images (Thermal & Visual)"));
    #endif
    status_led.setState(TAKING_DATA);
    // delay(1000); // Optional

    if (!captureImages(jpegImage, jpegLength, thermalData)) { // captureImages remains unchanged in signature
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Result: Image Task FAILED at Capture stage."));
        #endif
        // Error LED state and cleanup of partial captures handled within captureImages.
        // captureImages might need to log an error if it fails using ErrorLogger
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, "Image capture failed.");
        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Task Step: Images captured successfully."));
    #endif
    status_led.setState(ALL_OK); // Brief OK after capture, before send

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Task Step: Send Image Data"));
    #endif
    // status_led.setState(SENDING_DATA); // sendImageData will set this
    // delay(1000); // Optional

    // Pass cfg and api_obj to sendImageData
    if (!sendImageData(cfg, api_obj, *jpegImage, jpegLength, *thermalData)) { // MODIFIED CALL
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("Result: Image Task FAILED at Send stage."));
        #endif
        // Error LED state and logging handled within sendImageData
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("Result: Image Task SUCCEEDED (Capture & Send)."));
    #endif
    return true;
}

/**
 * @brief Performs final actions before entering deep sleep.
 * Sets the final status LED state based on the overall success of the loop cycle.
 * Performs a final visual blink sequence. Unmounts the LittleFS filesystem.
 * @param cycleStatusOK Boolean indicating if the main loop cycle completed without any detected errors.
 */
void prepareForSleep(bool cycleStatusOK) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("\n--- Preparing for Deep Sleep ---");
    #endif

    // Set final LED status to reflect the cycle outcome.
    if (cycleStatusOK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Cycle completed successfully. Setting final LED state to ALL_OK.");
        #endif
        led.setState(ALL_OK); // Indicate successful cycle.
        delay(1000); // Show OK state briefly.
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Cycle completed with errors. LED should reflect the last error encountered.");
            // The LED state should already be showing the last error (e.g., ERROR_SEND, ERROR_DATA).
        #endif
        // Keep the last error state visible for a moment.
        delay(3000);
    }

    // Perform a final blink sequence regardless of success/failure? Or only on success?
    // Current implementation blinks regardless.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Performing final LED blink sequence...");
    #endif
    ledBlink(); // Perform the blink sequence.

    // Unmount the LittleFS filesystem cleanly before sleeping.
    // This ensures filesystem integrity.
    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Unmounted LittleFS filesystem.");
    #endif
}


// =========================================================================
// ===             ENVIRONMENT TASK HELPER IMPLEMENTATIONS             ===
// =========================================================================

/**
 * @brief Reads the BH1750 light sensor value, attempting multiple times if necessary.
 * Calls the `lightSensor.readLightLevel()` method up to `SENSOR_READ_RETRIES` times
 * with a short delay between attempts until a valid reading (>= 0) is obtained.
 * @param[out] lightLevel Reference to a float variable where the successfully read light level (in lux) will be stored. Initialized to -1.0f on entry.
 * @return `true` if a valid light level was read successfully within the allowed retries.
 * `false` otherwise.
 */
bool readLightSensorWithRetry(float &lightLevel) {
    lightLevel = -1.0f; // Ensure initial state represents failure.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("  Reading light sensor (BH1750)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        lightLevel = lightSensor.readLightLevel(); // Attempt to read.
        // BH1750 library typically returns >= 0 for valid lux readings, negative on error.
        if (lightLevel >= 0.0f) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(" OK (" + String(lightLevel, 2) + " lx)"); // Print with 2 decimal places
            #endif
            return true; // Successful read.
        }
        // If read failed...
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("."); // Print dot to indicate retry attempt
        #endif
        delay(500); // Wait half a second before the next attempt.
    }
    // If loop completes without returning true, all retries failed.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(" FAILED after " + String(SENSOR_READ_RETRIES) + " retries.");
    #endif
    return false; // Indicate failure after all retries.
}


/**
 * @brief Reads temperature and humidity from the DHT22 sensor, attempting multiple times if necessary.
 * Calls `dhtSensor.readTemperature()` and `dhtSensor.readHumidity()` up to
 * `SENSOR_READ_RETRIES` times with a delay between attempts, until *both* readings
 * are valid (not NaN - Not a Number).
 * @param[out] temperature Reference to a float variable where the successfully read temperature (in degrees Celsius) will be stored. Initialized to NAN on entry.
 * @param[out] humidity Reference to a float variable where the successfully read relative humidity (in %) will be stored. Initialized to NAN on entry.
 * @return `true` if both temperature and humidity were read successfully as valid numbers within the allowed retries.
 * `false` otherwise.
 */
bool readDHTSensorWithRetry(float &temperature, float &humidity) {
    temperature = NAN; // Initialize output parameter to Not-a-Number (invalid state).
    humidity = NAN;    // Initialize output parameter to Not-a-Number (invalid state).
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("  Reading temp/humidity sensor (DHT22)...");
    #endif
    for (int i = 0; i < SENSOR_READ_RETRIES; ++i) {
        temperature = dhtSensor.readTemperature(); // Attempt to read temperature.
        // Short delay between temp and humidity reads might sometimes help DHT sensors.
        delay(100);
        humidity = dhtSensor.readHumidity();       // Attempt to read humidity.

        // Check if both readings are valid numbers (isnan() checks for Not-a-Number).
        if (!isnan(temperature) && !isnan(humidity)) {
            #ifdef ENABLE_DEBUG_SERIAL
                // Print results with 2 decimal places for temp, 1 for humidity.
                Serial.println(" OK (Temp: " + String(temperature, 2) + " C, Hum: " + String(humidity, 1) + " %)");
            #endif
            return true; // Both readings are valid, success.
        }
        // If one or both readings failed (are NaN)...
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("."); // Print dot to indicate retry attempt
        #endif
        // DHT sensors can be slow; wait longer between retries compared to I2C sensors.
        delay(1000); // Wait 1 second before the next attempt. (Adjust if needed based on sensor behavior)
    }
    // If loop completes without returning true, all retries failed.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(" FAILED after " + String(SENSOR_READ_RETRIES) + " retries.");
    #endif
    return false; // Indicate failure after all retries.
}


/**
 * @brief Sends the collected environmental data (light, temp, hum) to the configured server endpoint.
 * Uses the `EnvironmentDataJSON` utility class to format the data as JSON and send it
 * via HTTP POST. Manages LED status updates for the sending process (SENDING_DATA, ERROR_SEND).
 * Logs success or failure status (if debug enabled).
 * @param lightLevel The light level value (float, lux) to send.
 * @param temperature The temperature value (float, degrees Celsius) to send.
 * @param humidity The relative humidity value (float, %) to send.
 * @param cfg Reference to the global Config object containing API endpoint and path information.
 * @param api_obj Reference to the global API object for managing access tokens and API interactions.
 * @return `true` if the data was sent successfully (server returned HTTP 2xx).
 * `false` otherwise (HTTP request failed or server returned an error).
 */
bool sendEnvironmentDataToServer(Config& cfg, API& api_obj, float lightLevel, float temperature, float humidity) { 
    led.setState(SENDING_DATA);
    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiAmbientDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("  Sending environmental data via HTTP POST...");
        Serial.println("  Target URL: " + fullUrl);
    #endif

    int httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, lightLevel, temperature, humidity);

    if (httpCode == 200 || httpCode == 204) { // 204 No Content is typical for successful POSTs without a body response
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("  Environmental data sent successfully."));
        #endif
        // led.setState(ALL_OK); // Or let the main loop cycle handler do this
        return true;
    } else if (httpCode == 401 && api_obj.isActivated()) { // Token expired/invalid, and we are supposed to be active
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("  Env data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_WARNING, "Env data send returned 401. Attempting token refresh.");
        
        int refreshHttpCode = api_obj.performTokenRefresh();
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("  Token refresh successful. Re-trying env data send..."));
            #endif
            ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, "Token refreshed successfully after env data 401.");
            token = api_obj.getAccessToken(); // Get the new token
            httpCode = EnvironmentDataJSON::IOEnvironmentData(fullUrl, token, lightLevel, temperature, humidity);
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("  Environmental data sent successfully on retry."));
                #endif
                return true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("  Env data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Env data send failed on retry after refresh. HTTP: " + String(httpCode));
            }
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("  Token refresh failed after 401. HTTP Code: %d\n", refreshHttpCode);
            #endif
            // If refresh fails critically, api_obj.isActivated() might become false.
            // The main loop's next apiReady check will catch this.
             ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Token refresh failed after env data 401. Refresh HTTP: " + String(refreshHttpCode));
        }
    } else { // Other HTTP errors or client errors from IOEnvironmentData
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("  Error sending environmental data. HTTP Code: %d\n", httpCode);
        #endif
         ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, "Failed to send environmental data. HTTP Code: " + String(httpCode));
    }

    led.setState(ERROR_SEND);
    delay(1000); // Show error briefly
    return false;
}

// =========================================================================
// ===              IMAGE CAPTURE HELPER IMPLEMENTATIONS               ===
// =========================================================================

/**
 * @brief Reads a frame from the MLX90640 thermal sensor, allocates a new buffer
 * in PSRAM (if available) or heap, and copies the thermal data into it.
 * @param[out] thermalDataBuffer Pointer to a `float*` variable in the caller. On success,
 * this variable will be updated to point to the newly allocated buffer containing the copied thermal data (768 floats). Set to `nullptr` on failure.
 * @return `true` if the thermal frame was read, memory was allocated, and data was copied successfully.
 * `false` otherwise (read failure, allocation failure).
 */
bool captureAndCopyThermalData(float** thermalDataBuffer) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("  Reading thermal camera frame (MLX90640)...");
    #endif
    *thermalDataBuffer = nullptr; // Ensure output pointer is null initially.

    // Attempt to read a frame from the thermal sensor into its internal buffer.
    if (!thermalSensor.readFrame()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("  Error: Failed to read thermal frame from MLX90640 sensor.");
        #endif
        return false; // Read failure.
    }

    // Get a pointer to the sensor's internal buffer containing the latest frame data.
    float* rawThermalData = thermalSensor.getThermalData();
    if (rawThermalData == nullptr) {
        // This shouldn't happen if readFrame() succeeded, but check just in case.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("  Error: Failed to get thermal data pointer from sensor (pointer is null).");
        #endif
        return false; // Failed to get pointer.
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("  Thermal frame read OK. Allocating buffer for copy...");
    #endif

    // Calculate the required size for the buffer (768 float values).
    const size_t thermalDataSizeInBytes = 32 * 24 * sizeof(float);

    // Allocate memory for our copy of the thermal data.
    // Prioritize PSRAM if available using ps_malloc for potentially large data.
    // Fallback to standard malloc if PSRAM is not enabled/available.
    #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT // Check if PSRAM support is compiled in
        *thermalDataBuffer = (float*)ps_malloc(thermalDataSizeInBytes);
        #ifdef ENABLE_DEBUG_SERIAL
            // Serial.println("  Attempting allocation in PSRAM.");
        #endif
    #else
        *thermalDataBuffer = (float*)malloc(thermalDataSizeInBytes);
        #ifdef ENABLE_DEBUG_SERIAL
            // Serial.println("  Attempting allocation in standard heap (PSRAM not enabled/available).");
        #endif
    #endif

    // Check if memory allocation was successful.
    if (*thermalDataBuffer == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("  CRITICAL ERROR: Failed to allocate %d bytes for thermal data copy!\n", thermalDataSizeInBytes);
        #endif
        // Optional: Log this critical memory allocation failure.
        // ErrorLogger::sendLog(String(API_LOGGING), "MALLOC_FAIL", "Thermal data buffer allocation failed");
        return false; // Allocation failure.
    }

    // Allocation succeeded, now copy the data from the sensor's internal buffer
    // to our newly allocated buffer.
    memcpy(*thermalDataBuffer, rawThermalData, thermalDataSizeInBytes);
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("  Thermal data buffer allocated and frame data copied successfully.");
    #endif
    return true; // Indicate success.
}

/**
 * @brief Captures a JPEG image using the OV2640 camera sensor.
 * Calls the `camera.captureJPEG()` method, which handles buffer allocation
 * (in PSRAM) internally. The allocated buffer's address and size are returned
 * via the output parameters.
 * @param[out] jpegImageBuffer Pointer to a `uint8_t*` variable in the caller. On success,
 * this will be updated to point to the buffer allocated by the camera driver containing the JPEG data. Set to `nullptr` on failure.
 * @param[out] jpegLength Reference to a `size_t` variable in the caller. On success,
 * this will be updated with the size (in bytes) of the captured JPEG image. Set to 0 on failure.
 * @return `true` if the JPEG image was captured and the buffer allocated successfully.
 * `false` otherwise (capture failed or allocation failed).
 */
bool captureVisualJPEG(uint8_t** jpegImageBuffer, size_t& jpegLength) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("  Capturing visual JPEG image (OV2640)...");
    #endif
    // Ensure output parameters are reset before capture attempt.
    *jpegImageBuffer = nullptr;
    jpegLength = 0;

    // Call the camera wrapper's capture function.
    // This function internally calls esp_camera_fb_get(), ps_malloc(), memcpy(), esp_camera_fb_return().
    // It returns the pointer to the newly allocated buffer containing the JPEG data.
    *jpegImageBuffer = camera.captureJPEG(jpegLength);

    // Check if the capture and allocation were successful.
    // captureJPEG returns nullptr on failure, and jpegLength should be 0.
    if (*jpegImageBuffer == nullptr || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("  Error: Failed to capture JPEG image or memory allocation failed within camera driver/wrapper.");
        #endif
        // Optional: Log this failure.
        // ErrorLogger::sendLog(String(API_LOGGING), "JPEG_CAPTURE_FAIL", "JPEG capture or buffer allocation failed");
        return false; // Indicate failure.
    }

    // If capture succeeded.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("  JPEG Image captured successfully. Size: %d bytes.\n", jpegLength);
    #endif
    return true; // Indicate success.
}