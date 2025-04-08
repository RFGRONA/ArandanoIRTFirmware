/**
 * @file main.cpp
 * @brief Main application firmware for ESP32-S3 based environmental and plant monitoring device.
 *
 * This firmware initializes various sensors (DHT22, BH1750, MLX90640, OV2640),
 * loads configuration from LittleFS, connects to WiFi using loaded credentials,
 * periodically reads sensor data, captures thermal and visual images,
 * sends the data to configured API endpoints, and enters deep sleep between cycles.
 * Includes status indication via a WS2812 LED and optional serial debugging.
 */

// --- Core Arduino and System Includes ---
#include <Arduino.h>
#include <Wire.h>            // Required for I2C communication
#include "esp_heap_caps.h" // Required for memory checking/allocation (e.g., ps_malloc)
#include "esp_sleep.h"       // Required for deep sleep functions

// --- Filesystem & JSON Includes ---
#include <FS.h>              // Filesystem base class
#include <LittleFS.h>        // LittleFS filesystem implementation
#include <ArduinoJson.h>     // JSON parsing/serialization

// --- Optional Debugging ---
// Uncomment the following line to enable Serial output for debugging
// #define ENABLE_DEBUG_SERIAL
// --------------------------

// --- Local Libraries ---
#include "DHT22Sensor.h"
#include "BH1750Sensor.h"
#include "MLX90640Sensor.h"
#include "OV2640Sensor.h"
#include "LEDStatus.h"
#include "EnvironmentDataJSON.h"
#include "MultipartDataSender.h"
#include "WiFiManager.h"
// #include "ErrorLogger.h" // Commented out (backend not ready)
// #include "API.h"          // Commented out (backend not ready)

// --- Pin Definitions ---
#define I2C_SDA 47   ///< GPIO pin for I2C SDA line
#define I2C_SCL 21   ///< GPIO pin for I2C SCL line
#define DHT_PIN 14   ///< GPIO pin for DHT22 data line

// --- General Configuration (Defaults & Constants) ---
#define SENSOR_READ_RETRIES 3     ///< Number of attempts to read sensor data if initial read fails.
#define WIFI_CONNECT_TIMEOUT_MS 20000 ///< Timeout (in milliseconds) for establishing WiFi connection.
#define CONFIG_FILENAME "/config.json" ///< Filename for configuration on LittleFS

// --- Configuration Structure ---
/**
 * @brief Holds application configuration loaded from LittleFS or defaults.
 */
struct Config {
    String wifi_ssid = "DEFAULT_SSID"; ///< Default WiFi SSID if config load fails.
    String wifi_pass = "";             ///< Default WiFi Password.
    String api_env = "";               ///< Default Environmental API URL.
    String api_img = "";               ///< Default Image API URL.
    int sleep_sec = 60;                ///< Default sleep duration in seconds.
};
Config config; // Global configuration object
// -----------------------------

// --- I2C Bus Instance ---
// Using the default Wire object. Pins are set in Wire.begin().

// --- Global Object Instances ---
BH1750Sensor lightSensor(Wire, I2C_SDA, I2C_SCL); ///< Light sensor object using default Wire.
MLX90640Sensor thermalSensor(Wire);               ///< Thermal camera object using default Wire.
DHT22Sensor dhtSensor(DHT_PIN);                   ///< Temperature/Humidity sensor object.
OV2640Sensor camera;                              ///< Visual camera object.
LEDStatus led;                                    ///< Status LED object.
WiFiManager wifiManager;                          ///< WiFi connection manager object (constructor now takes no args).
// ErrorLogger errorLogger;                       ///< Error logger object (commented out for now).

// --- Function Prototypes ---
bool loadConfiguration(const char *filename);
void ledBlink();
bool initializeSensors();
bool readEnvironmentData();
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData);
void deepSleep(unsigned long seconds);
void cleanupResources(uint8_t* jpegImage, float* thermalData);
bool ensureWiFiConnected(unsigned long timeout);

// --- Helper Function Prototypes for Setup ---
void initSerial();
void initLED();
bool initFilesystem();
void loadConfigAndSetCredentials();
void handleSensorInitFailure();
// --- Helper Function Prototypes for Sensor Init ---
bool initDHTSensor();
bool initI2C();
bool initLightSensor();
bool initThermalSensor();
bool initCameraSensor();

// --- Helper Function Prototypes for Loop ---
bool handleWiFiConnection();
bool performEnvironmentTasks();
bool performImageTasks(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
void prepareForSleep(bool cycleStatusOK);

// --- Helper Function Prototypes for Environment Task ---
bool readLightSensorWithRetry(float &lightLevel);
bool readDHTSensorWithRetry(float &temperature, float &humidity);
bool sendEnvironmentDataToServer(float lightLevel, float temperature, float humidity);

// --- Helper Function Prototypes for Image Capture ---
bool captureAndCopyThermalData(float** thermalDataBuffer);
bool captureVisualJPEG(uint8_t** jpegImageBuffer, size_t& jpegLength);

// =========================================================================
// ===                          SETUP FUNCTION                           ===
// =========================================================================
/**
 * @brief Setup function, runs once on power-on or reset (including wake from deep sleep).
 * Initializes Serial (if debugging enabled), LED, LittleFS, loads configuration,
 * sets WiFi credentials, initializes sensors.
 * Enters deep sleep immediately if LittleFS mount or sensor initialization fails.
 */
void setup() {
  // Initialize LED first for early visual feedback
  initLED();

  // Initialize Serial port for debugging if enabled
  initSerial();

  // Initialize LittleFS filesystem, halt on critical failure
  if (!initFilesystem()) {
      // Specific error LED state might be set inside initFilesystem
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("CRITICAL: Halting due to filesystem error.");
      #endif
      while(1) { delay(1000); } // Halt execution
  }

  // Load configuration from file and set WiFi credentials
  loadConfigAndSetCredentials();

  // Initialize all sensors using the refactored function
  if (!initializeSensors()) {
       handleSensorInitFailure(); // Handles error LED and deep sleep logic
       // Execution stops in handleSensorInitFailure if it enters deep sleep
  }

  // Setup completed successfully
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Setup complete. Initial LED state OK.");
    Serial.println("--------------------------------------");
  #endif
  led.setState(ALL_OK); // Ensure final OK state for setup
  delay(1000);          // Brief delay showing OK status
}

// =========================================================================
// ===                           MAIN LOOP                               ===
// =========================================================================
/**
 * @brief Main loop function, runs repeatedly after setup.
 * Orchestrates the main workflow: WiFi, environment tasks, image tasks, cleanup, sleep.
 */
void loop() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("\n--- Starting Loop Cycle ---");
  #endif

  // Indicate Start of Cycle
  ledBlink();

  // Track overall success of the cycle
  bool everythingOK = true;

  // 1. Handle WiFi Connection (includes sleep on failure)
  if (!handleWiFiConnection()) {
      return; // Exit loop early if WiFi failed (already going to sleep)
  }

  // 2. Perform Environmental Data Task
  if (!performEnvironmentTasks()) {
      everythingOK = false; // Mark cycle as having errors
  }

  // 3. Perform Image Task (Capture, Send/Save)
  // Declare pointers here so they are in scope for cleanupResources
  uint8_t* jpegImage = nullptr;
  size_t jpegLength = 0;
  float* thermalData = nullptr;
  // Only attempt image tasks if previous steps were generally OK
  if (everythingOK) {
      if (!performImageTasks(&jpegImage, jpegLength, &thermalData)) {
          everythingOK = false; // Mark cycle as having errors
      }
  }

  // 4. Cleanup Resources (Memory buffers and Camera peripheral)
  // Pass the pointers, even if null, to the cleanup function.
  cleanupResources(jpegImage, thermalData);

  // 5. Prepare for Sleep (Final LED status, blink, unmount FS)
  prepareForSleep(everythingOK);

  // 6. Enter Deep Sleep
  deepSleep(config.sleep_sec);
}


// =========================================================================
// ===                      HELPER FUNCTIONS                           ===
// =========================================================================

/**
 * @brief Loads configuration from a JSON file on LittleFS.
 * Populates the global 'config' struct. Uses defaults if file not found or parsing fails.
 * @param filename The path to the configuration file (e.g., "/config.json").
 * @return True if the configuration was loaded successfully from the file, False otherwise.
 */
bool loadConfiguration(const char *filename) {
    File configFile = LittleFS.open(filename, "r");
    if (!configFile) {
        #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Failed to open config file: " + String(filename));
        #endif
        return false; // Indicate failure to open file
    }

    // Allocate a JsonDocument (adjust size if config becomes larger)
    // Use https://arduinojson.org/v6/assistant/ to estimate size
    JsonDocument doc;
    // For ESP32, dynamic allocation usually works well.
    // If memory becomes an issue, consider StaticJsonDocument with calculated capacity.

    // Parse the JSON file content
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close(); // Close the file promptly

    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("Failed to parse config file: ");
        Serial.println(error.c_str());
        #endif
        return false; // Indicate parsing failure
    }

    // Extract values, using defaults if keys are missing or null
    // Operator | provides default value if doc["key"] is null or missing
    config.wifi_ssid = doc["wifi_ssid"] | String("DEFAULT_SSID");
    config.wifi_pass = doc["wifi_pass"] | String("");
    config.api_env = doc["api_env"] | String("");
    config.api_img = doc["api_img"] | String("");
    config.sleep_sec = doc["sleep_sec"] | 60;

    #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Configuration loaded successfully:");
    Serial.println("  WiFi SSID: " + config.wifi_ssid);
    Serial.println("  WiFi Pass: ********"); // Avoid printing password
    Serial.println("  API Env: " + config.api_env);
    Serial.println("  API Img: " + config.api_img);
    Serial.println("  Sleep Sec: " + String(config.sleep_sec));
    #endif
    return true; // Indicate successful loading
}


/**
 * @brief Ensures WiFi is connected, attempting to connect if necessary.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
bool ensureWiFiConnected(unsigned long timeout) {
  if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
      return true;
  }
  if (wifiManager.getConnectionStatus() != WiFiManager::CONNECTING) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFi not connected. Attempting connection...");
      #endif
      wifiManager.connectToWiFi();
  }
  led.setState(CONNECTING_WIFI);
  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    wifiManager.handleWiFi();
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
        return true;
    }
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("WiFi connection definitely failed (max retries).");
        #endif
        break;
    }
    delay(100);
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("WiFi connection timed out or failed within ensureWiFiConnected.");
  #endif
  led.setState(ERROR_WIFI);
  return false;
}

/**
 * @brief Performs a simple LED blink sequence.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
void ledBlink() {
  led.setState(ALL_OK); delay(700);
  led.setState(OFF); delay(500);
  led.setState(ALL_OK); delay(700);
  led.setState(OFF); delay(500);
  led.setState(ALL_OK); delay(700);
  led.setState(OFF); delay(500);
}

/**
 * @brief Initializes all connected sensors in sequence.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
bool initializeSensors() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing DHT22...");
  #endif
  dhtSensor.begin();

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing I2C Bus (Pins SDA: " + String(I2C_SDA) + ", SCL: " + String(I2C_SCL) + ")...");
  #endif
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);
  delay(100);

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing BH1750 (Light Sensor)...");
  #endif
  if (!lightSensor.begin()) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! BH1750 Init FAILED !!!");
    #endif
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  delay(100);

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing MLX90640 (Thermal Sensor)...");
  #endif
  if (!thermalSensor.begin()) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! MLX90640 Init FAILED !!!");
    #endif
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Waiting for MLX90640 stabilization (~2s)...");
  #endif
  delay(2000);

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing OV2640 (Camera)...");
  #endif
  if (!camera.begin()) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! OV2640 Init FAILED !!!");
    #endif
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  delay(500);

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("All sensors initialized successfully.");
  #endif
  return true;
}

/**
 * @brief Reads environmental data (light, temp, humidity) with retries
 * and sends it to the configured API endpoint. Sets LED state and returns status.
 * This version orchestrates calls to helper functions for reading and sending.
 * @return True if data was read and sent successfully, False otherwise.
 */
bool readEnvironmentData() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("--- Reading Environment Sensors ---");
  #endif

  float light = -1.0; // Initialize with invalid value
  float temp = NAN;   // Initialize with invalid value
  float hum = NAN;    // Initialize with invalid value

  led.setState(TAKING_DATA); // Set LED to indicate data capture phase
  delay(1000); // Short delay before reading sensors
  
  // Read sensors using helper functions with retries
  bool lightOK = readLightSensorWithRetry(light);
  bool dhtOK = readDHTSensorWithRetry(temp, hum);

  // Check if all readings were successful
  if (!lightOK || !dhtOK) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Failed to read one or more environment sensors after retries.");
      #endif
      // Set a generic data error state; specific sensor read function might log details
      led.setState(ERROR_DATA);
      delay(3000); // Show error
      return false; // Indicate overall failure for this task
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("All environment sensors read successfully. Proceeding to send...");
    Serial.printf("  Read values: Light=%.2f lx, Temp=%.2f C, Hum=%.2f %%\n", light, temp, hum);
  #endif

  // Attempt to send the valid data using a helper function
  if (!sendEnvironmentDataToServer(light, temp, hum)) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Failed to send environment data to server.");
      #endif
      // Error LED state and delay are handled within sendEnvironmentDataToServer
      return false; // Indicate overall failure for this task
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Environment data sent successfully to server.");
  #endif
  return true; // Indicate overall success for this task
}

/**
 * @brief Captures thermal data (MLX90640) and a visual image (OV2640).
 * Orchestrates calls to helper functions for capturing and allocating memory.
 * Caller must free the allocated buffers (*thermalData and *jpegImage).
 * Sets LED state and returns status.
 *
 * @param[out] jpegImage Pointer to a uint8_t pointer, will be set to the allocated JPEG buffer address.
 * @param[out] jpegLength Reference to size_t, will be set to the JPEG buffer size.
 * @param[out] thermalData Pointer to a float pointer, will be set to the allocated thermal data buffer address.
 * @return True if both captures and allocations were successful, False otherwise.
 */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("--- Capturing Images ---");
  #endif
  // Ensure output pointers are null initially
  *jpegImage = nullptr;
  *thermalData = nullptr;
  jpegLength = 0;

  // 1. Capture and Copy Thermal Data
  if (!captureAndCopyThermalData(thermalData)) {
      // Error logged within helper function
      led.setState(ERROR_DATA);
      delay(3000);
      return false; // Thermal capture/copy failed
  }

  // 2. Capture Visual JPEG Data
  if (!captureVisualJPEG(jpegImage, jpegLength)) {
      // Error logged within helper function
      led.setState(ERROR_DATA);
      delay(3000);

      // *** CRITICAL: Free thermal data buffer if JPEG capture failed ***
      if (*thermalData != nullptr) {
          #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("Freeing thermal data buffer due to JPEG capture failure.");
          #endif
          free(*thermalData);
          *thermalData = nullptr; // Avoid dangling pointer
      }
      return false; // Visual capture failed
  }

  // If both captures succeeded
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Both thermal and visual images captured successfully.");
  #endif
  return true;
}

/**
 * @brief Sends the captured thermal and visual image data.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Sending thermal and image data via HTTP POST (multipart) to " + config.api_img + "...");
  #endif
  // Use configuration value for API URL
  bool sendSuccess = MultipartDataSender::IOThermalAndImageData(
    config.api_img, thermalData, jpegImage, jpegLength
  );
  delay(1000);

  if (!sendSuccess) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("HTTP POST for image data failed.");
    #endif
    led.setState(ERROR_SEND);
    delay(3000);
    return false;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Image data HTTP POST successful.");
  #endif
  return true;
}

/**
 * @brief Cleans up allocated resources (memory, camera).
 * (Documentation moved to header/previous example - implementation unchanged)
 */
void cleanupResources(uint8_t* jpegImage, float* thermalData) {
  if (jpegImage != nullptr) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Freeing JPEG image buffer...");
    #endif
    free(jpegImage);
  }
  if (thermalData != nullptr) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Freeing thermal data buffer...");
    #endif
    free(thermalData);
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Deinitializing camera (calling camera.end())...");
  #endif
  camera.end();
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Camera deinitialized.");
  #endif
  delay(50);
}

/**
 * @brief Enters ESP32 deep sleep mode.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
void deepSleep(unsigned long seconds) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.printf("Entering deep sleep for %lu seconds...\n", seconds);
    Serial.println("--------------------------------------");
    delay(100);
  #endif
  led.turnOffAll();
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_deep_sleep_start();
  // Execution stops here
}

// --- Setup Helper Function Implementations ---

/**
 * @brief Initializes the Serial port if debug is enabled.
 */
void initSerial() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.begin(115200);
    while (!Serial); // Optional: wait for serial connection
    uint64_t chipid = ESP.getEfuseMac();
    Serial.printf("\n--- Device Booting / Waking Up (ID: %04X%08X) ---\n", (uint16_t)(chipid>>32), (uint32_t)chipid);
  #endif
}

/**
* @brief Initializes the status LED.
*/
void initLED() {
  led.begin();
  led.setState(ALL_OK); // Set initial state
}

/**
* @brief Initializes the LittleFS filesystem.
* Attempts to mount, and formats if mounting fails initially.
* Sets error LED and returns false on critical failure.
* @return True if filesystem is mounted successfully, False otherwise.
*/
bool initFilesystem() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Mounting LittleFS...");
  #endif
  if (!LittleFS.begin(false)) { // false = don't format on fail initially
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("LittleFS mount failed! Trying to format...");
      #endif
      // If mount failed, try formatting and mounting again
      if (!LittleFS.begin(true)) { // true = format if mount fails
          #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("CRITICAL: Formatting LittleFS failed!");
          #endif
          led.setState(ERROR_DATA); // Use ERROR_DATA for FS failure
          delay(3000); // Show error
          return false; // Return critical failure
      } else {
          #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("LittleFS formatted successfully. Config file may need uploading.");
          #endif
          // Filesystem formatted, loadConfiguration will use defaults.
      }
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("LittleFS mounted successfully.");
  #endif
  return true; // Filesystem mounted
}

/**
* @brief Loads configuration from LittleFS and sets WiFi credentials.
* Prints status messages if debug is enabled. Uses default config values on failure.
*/
void loadConfigAndSetCredentials() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Loading configuration from " + String(CONFIG_FILENAME) + "...");
  #endif
  if (!loadConfiguration(CONFIG_FILENAME)) {
     #ifdef ENABLE_DEBUG_SERIAL
       Serial.println("Failed to load configuration or file not found. Using defaults.");
     #endif
     // Continue with default values stored in the global 'config' object
  }

  // Set WiFi credentials in the WiFiManager instance
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Setting WiFi credentials...");
  #endif
  wifiManager.setCredentials(config.wifi_ssid, config.wifi_pass);
}

/**
* @brief Handles the logic when sensor initialization fails.
* Sets error LED, unmounts LittleFS, and enters deep sleep.
*/
void handleSensorInitFailure() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Sensor initialization failed. Entering deep sleep.");
  #endif
  // LED state should have been set by the failing init helper function.
  LittleFS.end(); // Unmount filesystem before sleeping
  delay(500);     // Short delay
  deepSleep(config.sleep_sec); // Use sleep duration from config (or default)
  // Execution stops here.
}

// --- Loop Helper Function Implementations ---

/**
 * @brief Handles WiFi connection check at the start of the loop.
 * Calls ensureWiFiConnected and initiates sleep cycle immediately on failure.
 * @return True if WiFi is connected, False if connection failed (and sleep was initiated).
 */
bool handleWiFiConnection() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Checking WiFi connection status...");
  #endif
  if (!ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS)) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFi connection failed or timed out in loop. Entering sleep.");
      #endif
      delay(3000); // Show error color
      // Need cleanup before sleeping even if no buffers were allocated this cycle, primarily for camera
      cleanupResources(nullptr, nullptr); // Pass nullptrs as buffers aren't allocated yet
      LittleFS.end(); // Ensure filesystem is unmounted
      deepSleep(config.sleep_sec);
      // Execution stops here if deepSleep is entered
      return false; // Indicate connection failure
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("WiFi connection OK.");
  #endif
  return true; // Indicate connection success
}

/**
* @brief Performs the tasks related to reading and sending environmental data.
* Calls readEnvironmentData() and logs status.
* @return True if tasks were successful, False otherwise.
*/
bool performEnvironmentTasks() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("--- Performing Environment Tasks ---");
    Serial.println("Task: Read & Send Environmental Data");
  #endif

  if (!readEnvironmentData()) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task Failed: Read/Send Environmental Data.");
      #endif
      // Error LED state is set within readEnvironmentData on failure
      return false;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Task OK: Environmental data read and sent.");
  #endif
  return true;
}

/**
* @brief Performs tasks related to capturing and sending (or saving) image data.
* Calls captureImages, sendImageData, and potentially saveDataToFS. Manages LED states.
* @param[out] jpegImage Pointer to the JPEG image buffer pointer (will be allocated by captureImages).
* @param[out] jpegLength Reference to the JPEG image length.
* @param[out] thermalData Pointer to the thermal data buffer pointer (will be allocated by captureImages).
* @return True if tasks were successful (capture & send OK), False otherwise (capture failed OR send failed).
*/
bool performImageTasks(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("--- Performing Image Tasks ---");
    Serial.println("Task: Capture Images (Thermal & Visual)");
  #endif
  led.setState(TAKING_DATA); // Set LED for capture phase
  delay(1000); // Short delay before capturing
  // Ensure pointers are null initially
  *jpegImage = nullptr;
  *thermalData = nullptr;
  jpegLength = 0;

  // 1. Capture Images
  if (!captureImages(jpegImage, jpegLength, thermalData)) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task Failed: Capture Images.");
      #endif
      // Resources allocated within captureImages should be freed by it on failure
      return false; // Return failure status
  }
   #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Task OK: Images captured. Proceeding to send...");
   #endif

  // 2. Send Image Data (if capture was OK)
  led.setState(SENDING_DATA); // Set LED for sending phase
  delay(1000); // Short delay before sending

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Task: Send Image Data");
  #endif
  if (!sendImageData(*jpegImage, jpegLength, *thermalData)) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task Failed: Send Image Data. Attempting to save to FS (if implemented)...");
      #endif
      // Even if saving fails, we return false because the primary send failed.
      return false; // Return failure status
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Task OK: Image data sent successfully.");
  #endif
  return true; // All image tasks successful
}

/**
* @brief Prepares the device for deep sleep.
* Sets the final LED status based on cycle success, performs a final blink,
* and unmounts the LittleFS filesystem.
* @param cycleStatusOK Indicates if the main loop cycle completed without errors.
*/
void prepareForSleep(bool cycleStatusOK) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("--- Preparing for Sleep ---");
  #endif

  // Set final LED status
  if (cycleStatusOK) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Cycle OK. Setting LED to ALL_OK.");
      #endif
  } else {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Cycle finished with errors. LED should show last error state.");
      #endif
  }

  // Perform final blink sequence
  ledBlink();

  // Unmount LittleFS before sleeping
  LittleFS.end();
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Unmounted LittleFS.");
  #endif
}

// --- Environment Task Helper Function Implementations ---

/**
 * @brief Reads the light sensor (BH1750) value with retries.
 * @param[out] lightLevel Reference to store the successfully read light level (in lux).
 * @return True if read successfully within retries, False otherwise.
 */
bool readLightSensorWithRetry(float &lightLevel) {
  lightLevel = -1.0; // Ensure initial invalid state
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.print("Reading light sensor");
  #endif
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
      lightLevel = lightSensor.readLightLevel();
      if (lightLevel >= 0) { // BH1750 library returns >= 0 on success
          #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(" OK (" + String(lightLevel) + " lx)");
          #endif
          return true; // Success
      }
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.print(".");
      #endif
      delay(500); // Wait before retry
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println(" FAILED after retries.");
  #endif
  return false; // Failed after all retries
}

/**
* @brief Reads the temperature and humidity sensor (DHT22) values with retries.
* @param[out] temperature Reference to store the successfully read temperature (°C).
* @param[out] humidity Reference to store the successfully read humidity (%).
* @return True if both values were read successfully within retries, False otherwise.
*/
bool readDHTSensorWithRetry(float &temperature, float &humidity) {
  temperature = NAN; // Ensure initial invalid state
  humidity = NAN;    // Ensure initial invalid state
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.print("Reading DHT22 sensor");
  #endif
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
      temperature = dhtSensor.readTemperature();
      humidity = dhtSensor.readHumidity();
      // Check if both readings are valid numbers
      if (!isnan(temperature) && !isnan(humidity)) {
          #ifdef ENABLE_DEBUG_SERIAL
             Serial.println(" OK (" + String(temperature) + "C, " + String(humidity) + "%)");
          #endif
          return true; // Success
      }
       #ifdef ENABLE_DEBUG_SERIAL
        Serial.print(".");
      #endif
      delay(1000); // Wait longer for DHT retry
  }
   #ifdef ENABLE_DEBUG_SERIAL
     Serial.println(" FAILED after retries.");
   #endif
  return false; // Failed after all retries
}

/**
* @brief Sends the collected environmental data to the server.
* Handles setting LED state and logging for the send operation.
* @param lightLevel The light level value to send.
* @param temperature The temperature value to send.
* @param humidity The humidity value to send.
* @return True if the data was sent successfully (HTTP 2xx), False otherwise.
*/
bool sendEnvironmentDataToServer(float lightLevel, float temperature, float humidity) {
  led.setState(SENDING_DATA); // Set LED to sending status
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Sending environmental data via HTTP POST to " + config.api_env + "...");
  #endif

  // Use configuration value for API URL
  bool sendSuccess = EnvironmentDataJSON::IOEnvironmentData(
      config.api_env, lightLevel, temperature, humidity
  );
  delay(1000); // Short delay after sending attempt

  if (!sendSuccess) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("HTTP POST for environmental data failed.");
      #endif
      led.setState(ERROR_SEND); // Set LED to send error
      delay(3000); // Show error
      // ErrorLogger::sendLog(String(API_LOGGING), "ENV_SEND_FAIL", "Environment data send failed"); // Currently commented out
      return false; // Indicate failure
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Environmental data HTTP POST successful.");
  #endif
  // Optionally set LED back to a 'processing' state or leave as SENDING for now
  // led.setState(TAKING_DATA); // Or ALL_OK briefly? Depends on flow.
  return true; // Indicate success
}

// --- Image Capture Helper Function Implementations ---

/**
 * @brief Reads a frame from the thermal sensor, allocates memory, and copies the data.
 * @param[out] thermalDataBuffer Pointer to a float pointer, will be allocated and filled.
 * @return True on success, False on failure (read, alloc, or copy).
 */
bool captureAndCopyThermalData(float** thermalDataBuffer) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Reading thermal frame...");
  #endif
  if (!thermalSensor.readFrame()) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Failed to read thermal frame.");
     #endif
     *thermalDataBuffer = nullptr; // Ensure null on failure
     return false;
  }

  float* rawThermalData = thermalSensor.getThermalData();
  if (rawThermalData == nullptr) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Failed to get thermal data pointer (null).");
     #endif
     *thermalDataBuffer = nullptr;
     return false;
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Thermal frame read OK. Allocating buffer...");
  #endif

  // Allocate memory for the copy
  const size_t thermalSize = 32 * 24 * sizeof(float);
  #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT
    *thermalDataBuffer = (float*)ps_malloc(thermalSize);
  #else
    *thermalDataBuffer = (float*)malloc(thermalSize);
  #endif

  if (*thermalDataBuffer == nullptr) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! Failed to allocate memory for thermal data copy !!!");
     #endif
     // ErrorLogger::sendLog(String(API_LOGGING), "MALLOC_FAIL", "Thermal data malloc failed");
     return false;
  }

  // Copy data to the newly allocated buffer
  memcpy(*thermalDataBuffer, rawThermalData, thermalSize);
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Thermal data buffer allocated and copied.");
  #endif
  return true;
}

/**
* @brief Captures a JPEG image using the camera.
* The camera library allocates the buffer.
* @param[out] jpegImageBuffer Pointer to a uint8_t pointer, set to the allocated buffer.
* @param[out] jpegLength Reference set to the size of the captured image.
* @return True on success, False on failure.
*/
bool captureVisualJPEG(uint8_t** jpegImageBuffer, size_t& jpegLength) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Capturing JPEG image...");
  #endif
  // captureJPEG allocates memory; caller of captureImages is responsible for freeing jpegImageBuffer
  *jpegImageBuffer = camera.captureJPEG(jpegLength);

  if (*jpegImageBuffer == nullptr || jpegLength == 0) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! Failed to capture JPEG image or allocation failed !!!");
     #endif
     // ErrorLogger::sendLog(String(API_LOGGING), "JPEG_CAPTURE_FAIL", "JPEG capture failed");
     return false;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.printf("JPEG Image captured successfully (%d bytes).\n", jpegLength);
  #endif
  return true;
}
