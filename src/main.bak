#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "esp_heap_caps.h"

#include "DHT22Sensor.h"
#include "BH1750Sensor.h"
#include "MLX90640Sensor.h"
#include "OV2640Sensor.h"
#include "LEDStatus.h"
#include "EnvironmentDataJSON.h"
#include "MultipartDataSender.h"
#include "WiFiManager.h"
#include "ErrorLogger.h"

// Definición de pines
#define I2C_SDA 47
#define I2C_SCL 21
#define DHT_PIN 14

// Configuración general
#define SLEEP_DURATION_SECONDS 60
#define SENSOR_READ_RETRIES 3
#define WIFI_CONNECT_TIMEOUT_MS 20000 // Tiempo máximo para esperar conexión WiFi

// Configuración de WiFi
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

// Endpoints de la API
const char* API_ENVIRONMENTAL = "http://192.168.1.4:5000/ambiente";
const char* API_THERMAL = "http://192.168.1.4:5000/imagenes";
const char* API_LOGGING = "http://192.168.1.4:5000/log";

TwoWire i2cBus = TwoWire(0);

// Instancias de sensores y componentes
BH1750Sensor lightSensor(i2cBus, I2C_SDA, I2C_SCL);
MLX90640Sensor thermalSensor(i2cBus);
DHT22Sensor dhtSensor(DHT_PIN);
OV2640Sensor camera;
LEDStatus led;
WiFiManager wifiManager(WIFI_SSID, WIFI_PASS);
// ErrorLogger errorLogger; // Asumiendo que es estática o no necesita instancia

// Prototipos de funciones (Manteniendo tus firmas originales)
void ledBlink();
bool initializeSensors();
bool readEnvironmentData();
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
// bool sendEnvironmentData(JsonDocument& doc); // Comentado si no se usa
bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData);
void deepSleep(unsigned long seconds);
void cleanupResources(uint8_t* jpegImage, float* thermalData);
bool ensureWiFiConnected(unsigned long timeout); // Función auxiliar para WiFi

void setup() {
  led.begin();

  // Inicializar WiFi (solo modo)
  wifiManager.begin();

  // Inicializar sensores
  if (!initializeSensors()) {
    delay(500);
    deepSleep(SLEEP_DURATION_SECONDS);
    return;
  }

  led.setState(ALL_OK);
  delay(1000);
}

void loop() {
  uint8_t* jpegImage = nullptr;
  size_t jpegLength = 0;
  float* thermalData = nullptr;
  bool wifiConnectedThisCycle = false; // Flag para saber si se conectó en este ciclo
  bool everythingOK = true; // Asume éxito hasta que algo falle

  ledBlink();

  // --- INTENTO DE CONEXIÓN WIFI (con timeout) ---
  wifiConnectedThisCycle = ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS);

  // Si el WiFi falló en conectar después del timeout, poner LED de error y dormir.
  if (!wifiConnectedThisCycle) {
      led.setState(ERROR_WIFI);
      delay(3000);
      cleanupResources(jpegImage, thermalData); // Asegurar limpieza aunque no se haya capturado nada
      deepSleep(SLEEP_DURATION_SECONDS);
      return; // Salir del loop y dormir
  }

  // --- Lógica principal ---
  led.setState(TAKING_DATA);
  delay(1000);

  // Leer y enviar datos ambientales (Función original)
  if (!readEnvironmentData()) {
    // readEnvironmentData ya maneja el LED de error
    everythingOK = false;
    // Decisión: ¿continuar o dormir? Aquí continuamos para intentar la cámara.
  }

  // Capturar imágenes
  if (!captureImages(&jpegImage, jpegLength, &thermalData)) {
    led.setState(ERROR_DATA); delay(3000);
    everythingOK = false;
  } else {
    // Captura OK, intentar enviar datos de imágenes
    led.setState(SENDING_DATA);
    delay(1000);
    if (!sendImageData(jpegImage, jpegLength, thermalData)) {
       // sendImageData ya maneja el LED de error
      everythingOK = false;
    }
    // Si el envío es exitoso, everythingOK sigue como estaba
  }

  // --- Limpieza y Sueño ---
  cleanupResources(jpegImage, thermalData);

  // Estado final antes de dormir
  if (everythingOK) {
      led.setState(ALL_OK);
      delay(3000);
  } else {
      // Si algo falló (lectura, captura, envío), el LED ya está en estado de error
      delay(3000); // Mantener LED de error visible
  }

  ledBlink();
  deepSleep(SLEEP_DURATION_SECONDS);
}


// --- Nueva Función ensureWiFiConnected ---
bool ensureWiFiConnected(unsigned long timeout) {
  if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
      // led.setState(ALL_OK); // El LED se manejará después de llamar a esta función
      return true;
  }

  if (wifiManager.getConnectionStatus() != WiFiManager::CONNECTING) {
      wifiManager.connectToWiFi();
  }
  led.setState(CONNECTING_WIFI);

  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    wifiManager.handleWiFi();
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
        // led.setState(ALL_OK); // El LED se manejará después
        return true;
    }
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
        break; // Salir si el fallo es definitivo
    }
    delay(100);
  }

  // Si salimos por timeout o fallo definitivo
  // led.setState(ERROR_WIFI); // El LED se manejará después
  // delay(3000);
  return false;
}


// --- TUS FUNCIONES ORIGINALES (Sin cambios) ---

void ledBlink() {
  led.setState(OFF); delay(500);
  led.setState(ALL_OK); delay(700);
  led.setState(OFF); delay(500);
  led.setState(ALL_OK); delay(700);
  led.setState(OFF); delay(500);
}

bool initializeSensors() {
  dhtSensor.begin();
  i2cBus.begin(I2C_SDA, I2C_SCL);
  i2cBus.setClock(100000);
  delay(100);
  if (!lightSensor.begin()) {
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  delay(100);
  if (!thermalSensor.begin()) {
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  delay(500);
  if (!camera.begin()) {
    led.setState(ERROR_SENSOR); delay(3000); return false;
  }
  delay(500);
  return true;
}

bool readEnvironmentData() {
  float lightLevel = -1;
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    lightLevel = lightSensor.readLightLevel();
    if (lightLevel >= 0) break;
    delay(500);
  }
  float temperature = NAN;
  float humidity = NAN;
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    temperature = dhtSensor.readTemperature();
    humidity = dhtSensor.readHumidity();
    if (!isnan(temperature) && !isnan(humidity)) break;
    delay(1000);
  }
  if (lightLevel < 0 || isnan(temperature) || isnan(humidity)) {
    led.setState(ERROR_DATA); delay(3000);
    // ErrorLogger::sendLog(String(API_LOGGING), "ENV_READ_FAIL", "Sensor read failed"); // Comentario original
    return false;
  }
  led.setState(SENDING_DATA);
  bool sendSuccess = EnvironmentDataJSON::IOEnvironmentData(
    String(API_ENVIRONMENTAL), lightLevel, temperature, humidity
  );
  delay(1000);
  if (!sendSuccess) {
    led.setState(ERROR_SEND); delay(3000);
    // ErrorLogger::sendLog(String(API_LOGGING), "ENV_SEND_FAIL", "Environment data send failed"); // Comentario original
    return false;
  }
  return true;
}

bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  if (!thermalSensor.readFrame()) {
     led.setState(ERROR_DATA); delay(3000); return false;
  }
  float* rawThermalData = thermalSensor.getThermalData();
  if (rawThermalData == nullptr) {
    led.setState(ERROR_DATA); delay(3000); return false;
  }
  #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT
    *thermalData = (float*)ps_malloc(32 * 24 * sizeof(float));
  #else
    *thermalData = (float*)malloc(32 * 24 * sizeof(float));
  #endif
  if (*thermalData == nullptr) {
    // ErrorLogger::sendLog(String(API_LOGGING), "MALLOC_FAIL", "Thermal data malloc failed"); // Comentario original
    led.setState(ERROR_DATA); delay(3000); return false;
  }
  memcpy(*thermalData, rawThermalData, 32 * 24 * sizeof(float));
  *jpegImage = camera.captureJPEG(jpegLength);
  if (*jpegImage == nullptr || jpegLength == 0) {
    if (*thermalData != nullptr) { free(*thermalData); *thermalData = nullptr; }
    // ErrorLogger::sendLog(String(API_LOGGING), "JPEG_CAPTURE_FAIL", "JPEG capture failed"); // Comentario original
    led.setState(ERROR_DATA); delay(3000); return false;
  }
  return true;
}

bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData) {
  // led.setState(SENDING_DATA) // Comentario original - ya se hace en loop
  bool sendSuccess = MultipartDataSender::IOThermalAndImageData(
    String(API_THERMAL), thermalData, jpegImage, jpegLength
  );
  delay(1000);
  if (!sendSuccess) {
    led.setState(ERROR_SEND); delay(3000);
    // ErrorLogger::sendLog(String(API_LOGGING), "IMAGE_SEND_FAIL", "Image data send failed"); // Comentario original
    return false;
  }
  return true;
}

void cleanupResources(uint8_t* jpegImage, float* thermalData) {
  if (jpegImage != nullptr) { free(jpegImage); }
  if (thermalData != nullptr) { free(thermalData); }
}

void deepSleep(unsigned long seconds) {
  led.turnOffAll();
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  esp_deep_sleep_start();
}