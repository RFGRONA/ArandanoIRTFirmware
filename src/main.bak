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
  // Initialize status LED first
  led.begin();
  led.setState(ALL_OK); // Start with OK indication

  // Initialize Serial port for debugging if enabled
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.begin(115200);
    uint64_t chipid = ESP.getEfuseMac(); // Use Efuse Mac for a unique ID
    Serial.printf("\n--- Device Booting / Waking Up (ID: %04X%08X) ---\n", (uint16_t)(chipid>>32), (uint32_t)chipid);
  #endif

  // Initialize LittleFS filesystem
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Mounting LittleFS...");
  #endif
  if (!LittleFS.begin(false)) { // Pass 'false' to not format on fail first time
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("LittleFS mount failed! Trying to format...");
    #endif
    // If mount failed, try formatting and mounting again
    if (!LittleFS.begin(true)) { // Pass 'true' to format if mount fails
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("Formatting LittleFS failed! Halting.");
        #endif
        led.setState(ERROR_DATA); // Use ERROR_DATA for filesystem failure indication
        // Halt execution - cannot proceed without filesystem for config
        while(1) { delay(1000); }
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("LittleFS formatted successfully.");
        #endif
        // Filesystem formatted, but config file won't exist yet.
        // loadConfiguration will use defaults. Consider saving defaults here if needed.
    }
  } else {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("LittleFS mounted successfully.");
      #endif
  }

  // Load configuration from LittleFS
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Loading configuration from " + String(CONFIG_FILENAME) + "...");
  #endif
  if (!loadConfiguration(CONFIG_FILENAME)) {
     #ifdef ENABLE_DEBUG_SERIAL
       Serial.println("Failed to load configuration or file not found. Using defaults.");
     #endif
     // Continue with default values stored in the global 'config' object
     // Optionally, could set an error LED here if config is critical.
  }

  // Initialize WiFi Manager (sets mode) and set credentials from loaded config
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing WiFi Manager and setting credentials...");
  #endif
  wifiManager.begin();
  wifiManager.setCredentials(config.wifi_ssid, config.wifi_pass);

  // Initialize all sensors
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Initializing Sensors...");
  #endif
  if (!initializeSensors()) {
    // If sensors fail to initialize, indicate error and go directly to sleep
    // LED state is set within initializeSensors() on failure.
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Sensor initialization failed. Entering deep sleep.");
    #endif
    LittleFS.end(); // Unmount filesystem before sleeping
    delay(500);     // Short delay before sleeping
    deepSleep(config.sleep_sec); // Use sleep duration from config (or default)
    return;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Setup complete. Initial LED state OK.");
    Serial.println("--------------------------------------");
  #endif
  led.setState(ALL_OK); // Set final OK state for setup
  delay(1000);          // Brief delay showing OK status
}

// =========================================================================
// ===                           MAIN LOOP                               ===
// =========================================================================
/**
 * @brief Main loop function, runs repeatedly after setup.
 * Handles the main workflow: WiFi connection, data acquisition, data sending, cleanup, sleep.
 */
void loop() {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("\n--- Starting Loop Cycle ---");
  #endif

  // Pointers for dynamically allocated image/data buffers
  uint8_t* jpegImage = nullptr;
  size_t jpegLength = 0;
  float* thermalData = nullptr;

  bool wifiConnectedThisCycle = false; // Flag to track if WiFi connection succeeded this cycle
  bool everythingOK = true;            // Flag to track overall success of the cycle, assumes OK initially

  // --- Indicate Start of Cycle ---
  ledBlink(); // Visual indication that a new cycle is starting

  // --- Ensure WiFi Connection ---
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Ensuring WiFi connection...");
  #endif
  wifiConnectedThisCycle = ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS);

  // If WiFi failed, set error state, cleanup, and sleep
  if (!wifiConnectedThisCycle) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("WiFi connection failed or timed out. Entering sleep.");
      #endif
      delay(3000); // Show ERROR_WIFI color set by ensureWiFiConnected
      cleanupResources(jpegImage, thermalData);
      LittleFS.end(); // Unmount filesystem before sleeping
      deepSleep(config.sleep_sec);
      return; // Exit loop and sleep
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("WiFi connected successfully.");
  #endif

  // --- Main Data Acquisition and Sending Logic ---
  led.setState(TAKING_DATA);
  delay(1000);

  // 1. Read and Send Environmental Data
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Task: Read & Send Environmental Data");
  #endif
  if (!readEnvironmentData()) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Task Failed: Read/Send Environmental Data.");
    #endif
    everythingOK = false;
  } else {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Task OK: Environmental data sent.");
    #endif
  }

  // 2. Capture Images (only if previous steps were OK)
  if (everythingOK) {
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Task: Capture Images (Thermal & Visual)");
    #endif
    if (!captureImages(&jpegImage, jpegLength, &thermalData)) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task Failed: Capture Images.");
      #endif
      everythingOK = false;
    } else {
       #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task OK: Images captured.");
      #endif
      // 3. Send Image Data (only if capture was OK)
      led.setState(SENDING_DATA);
      delay(1000);
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Task: Send Image Data");
      #endif
      if (!sendImageData(jpegImage, jpegLength, thermalData)) {
        #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("Task Failed: Send Image Data.");
        #endif
        everythingOK = false;
      } else {
          #ifdef ENABLE_DEBUG_SERIAL
          Serial.println("Task OK: Image data sent.");
          #endif
      }
    }
  }

  // --- Cleanup and Sleep Preparation ---
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Cleaning up resources...");
  #endif
  cleanupResources(jpegImage, thermalData);

  // Set final LED status
  if (everythingOK) {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Loop cycle completed successfully.");
      #endif
      led.setState(ALL_OK);
      delay(3000);
  } else {
      #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("Loop cycle finished with errors.");
      #endif
      delay(3000); // Show last error color
  }

  ledBlink(); // Final blink

  // Unmount filesystem before sleeping
  LittleFS.end();
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Unmounted LittleFS.");
  #endif

  // --- Enter Deep Sleep ---
  deepSleep(config.sleep_sec); // Use sleep duration from config
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
 * @brief Reads environmental data and sends it to the API.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
bool readEnvironmentData() {
  float lightLevel = -1;
  float temperature = NAN;
  float humidity = NAN;

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.print("Reading light sensor");
  #endif
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    lightLevel = lightSensor.readLightLevel();
    if (lightLevel >= 0) break;
    #ifdef ENABLE_DEBUG_SERIAL
      Serial.print(".");
    #endif
    delay(500);
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println(lightLevel >= 0 ? " OK (" + String(lightLevel) + " lx)" : " FAILED");
  #endif

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.print("Reading DHT22 sensor");
  #endif
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    temperature = dhtSensor.readTemperature();
    humidity = dhtSensor.readHumidity();
    if (!isnan(temperature) && !isnan(humidity)) break;
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.print(".");
    #endif
    delay(1000);
  }
   #ifdef ENABLE_DEBUG_SERIAL
    if (!isnan(temperature) && !isnan(humidity)) {
       Serial.println(" OK (" + String(temperature) + "C, " + String(humidity) + "%)");
    } else {
       Serial.println(" FAILED");
    }
  #endif

  if (lightLevel < 0 || isnan(temperature) || isnan(humidity)) {
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }

  led.setState(SENDING_DATA);
   #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Sending environmental data via HTTP POST to " + config.api_env + "...");
  #endif
  // Use configuration value for API URL
  bool sendSuccess = EnvironmentDataJSON::IOEnvironmentData(
    config.api_env, lightLevel, temperature, humidity
  );
  delay(1000);

  if (!sendSuccess) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("HTTP POST for environmental data failed.");
    #endif
    led.setState(ERROR_SEND);
    delay(3000);
    return false;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Environmental data HTTP POST successful.");
  #endif
  return true;
}

/**
 * @brief Captures thermal and visual images.
 * (Documentation moved to header/previous example - implementation unchanged)
 */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Reading thermal frame...");
  #endif
  if (!thermalSensor.readFrame()) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Failed to read thermal frame.");
    #endif
     led.setState(ERROR_DATA); delay(3000); return false;
  }
  float* rawThermalData = thermalSensor.getThermalData();
  if (rawThermalData == nullptr) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("Failed to get thermal data pointer (null).");
    #endif
     led.setState(ERROR_DATA); delay(3000); return false;
  }
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Thermal frame read OK. Allocating buffer...");
  #endif

  #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT
    *thermalData = (float*)ps_malloc(32 * 24 * sizeof(float));
  #else
    *thermalData = (float*)malloc(32 * 24 * sizeof(float));
  #endif

  if (*thermalData == nullptr) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! Failed to allocate memory for thermal data copy !!!");
    #endif
    led.setState(ERROR_DATA); delay(3000); return false;
  }
  // Copy thermal data. Destination buffer (*thermalData) was allocated immediately
  // before with the exact size required (32 * 24 * sizeof(float)), so overflow is not possible.
  memcpy(*thermalData, rawThermalData, 32 * 24 * sizeof(float));
  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Thermal data buffer allocated and copied.");
  #endif

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.println("Capturing JPEG image...");
  #endif
  *jpegImage = camera.captureJPEG(jpegLength);
  if (*jpegImage == nullptr || jpegLength == 0) {
     #ifdef ENABLE_DEBUG_SERIAL
      Serial.println("!!! Failed to capture JPEG image or allocation failed !!!");
    #endif
    if (*thermalData != nullptr) {
       #ifdef ENABLE_DEBUG_SERIAL
         Serial.println("Freeing thermal data buffer due to JPEG capture failure.");
       #endif
       free(*thermalData);
       *thermalData = nullptr;
    }
    led.setState(ERROR_DATA); delay(3000); return false;
  }

  #ifdef ENABLE_DEBUG_SERIAL
    Serial.printf("JPEG Image captured successfully (%d bytes).\n", jpegLength);
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