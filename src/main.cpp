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
#define I2C_SDA 42
#define I2C_SCL 41
#define DHT_PIN 47

// Configuración general
#define SLEEP_DURATION_SECONDS 60   // Tiempo de sueño profundo en segundos
#define SENSOR_READ_RETRIES 3       // Intentos de lectura para sensores
#define WIFI_CONNECT_TIMEOUT_MS 20000 // Timeout de conexión WiFi

// Configuración de WiFi
const char* WIFI_SSID = "ZNEGVARM";
const char* WIFI_PASS = "L1E368B9G1I";

// Endpoints de la API
const char* API_ENVIRONMENTAL = "http://192.168.1.4:5000/ambiente"; 
const char* API_THERMAL = "http://192.168.1.4:5000/imagenes"; 
const char* API_LOGGING = "http://<IP_DEL_SERVIDOR_FLASK>:5000/log";
// ErrorLogger::sendLog(String(API_LOGGING), "ENV_DATA_FAIL", "Failed during environment read/send");

TwoWire i2cBus = TwoWire(0);

// Instancias de sensores y componentes
BH1750Sensor lightSensor(i2cBus, I2C_SDA, I2C_SCL);
MLX90640Sensor thermalSensor(i2cBus);
DHT22Sensor dhtSensor(DHT_PIN);
OV2640Sensor camera;
LEDStatus led;
WiFiManager wifiManager(WIFI_SSID, WIFI_PASS);

// Prototipos de funciones
void ledBlink();
bool initializeSensors();
bool readEnvironmentData();
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
bool sendEnvironmentData(JsonDocument& doc);
bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData);
void deepSleep(unsigned long seconds);
void cleanupResources(uint8_t* jpegImage, float* thermalData);
bool ensureWiFiConnected(unsigned long timeout); // Función auxiliar para WiF

void setup() {
  // Iniciar el LED de estado
  led.begin();

  // Inicializar WiFi}
  wifiManager.begin();
  
  // Inicializar sensores
  if (!initializeSensors()) {   
    delay(500);
    deepSleep(SLEEP_DURATION_SECONDS);
    return;
  }

  delay(1000);
}

void loop() {
  // Variables para almacenar datos
  uint8_t* jpegImage = nullptr;
  size_t jpegLength = 0;
  float* thermalData = nullptr;
  bool everythingOK = true; 
  bool wifiConnectedThisCycle = false;

  ledBlink(); // Parpadeo inicial del LED para indicar que el ESP32 está despierto

  wifiConnectedThisCycle = ensureWiFiConnected(WIFI_CONNECT_TIMEOUT_MS);

  // Si el WiFi falló en conectar después del timeout, poner LED de error y dormir.
  if (!wifiConnectedThisCycle) {
    led.setState(ERROR_WIFI);
    delay(3000);
    cleanupResources(jpegImage, thermalData); // Asegurar limpieza aunque no se haya capturado nada
    deepSleep(SLEEP_DURATION_SECONDS);
    return; // Salir del loop y dormir
}
  
  // Si llegamos aquí, WiFi está OK. Proceder con datos.
  led.setState(TAKING_DATA);
  delay(3000); 

  // Leer y enviar datos ambientales
  if (!readEnvironmentData()) {
    // readEnvironmentData ya establece el LED de error (SENSOR o SEND)
    everythingOK = false; // Marcar que el ciclo no fue completamente exitoso
    // Ir directamente a limpieza y dormir en caso de error ambiental
    cleanupResources(jpegImage, thermalData);
    deepSleep(SLEEP_DURATION_SECONDS);
    return; // Salir del loop y dormir
  }

  // Capturar imágenes (térmica y cámara)
  if (!captureImages(&jpegImage, jpegLength, &thermalData)) {
    everythingOK = false; // Marcar fallo
    // No enviar datos, proceder a limpieza y dormir
  } else {
    // Captura OK, intentar enviar datos de imágenes
    led.setState(SENDING_DATA);
    if (!sendImageData(jpegImage, jpegLength, thermalData)) {
      everythingOK = false; // Marcar fallo
    } else {
      // Captura y envío de imagen OK
      // everythingOK permanece true (asumiendo que lectura ambiental también fue OK)
    }
  }
  
  // Limpieza de recursos (siempre se ejecuta antes de dormir)
  cleanupResources(jpegImage, thermalData);
  
  // Indicar estado final antes de dormir
  if (everythingOK) {
    led.setState(ALL_OK);
    delay(3000); 
  } 

  ledBlink(); // Parpadeo final del LED para indicar que el ESP32 está a punto de dormir
  
  // Dormir y reiniciar
  deepSleep(SLEEP_DURATION_SECONDS);
}

/**
 * Parpadea el LED de estado para indicar que el ESP32 está en modo de espera.
 */
void ledBlink() {
  led.setState(OFF);
  delay(500);
  led.setState(ALL_OK);
  delay(700);
  led.setState(OFF);
  delay(500);
  led.setState(ALL_OK);
  delay(700);
  led.setState(OFF);
  delay(500);
}

bool ensureWiFiConnected(unsigned long timeout) {
  led.setState(CONNECTING_WIFI); 
  if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
      return true;
  }

  if (wifiManager.getConnectionStatus() != WiFiManager::CONNECTING) {
      wifiManager.connectToWiFi();
  }

  unsigned long startTime = millis();
  while (millis() - startTime < timeout) {
    wifiManager.handleWiFi();
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTED) {
        led.setState(CONNECTING_WIFI);
        delay(2000);
        return true;
    }
    if (wifiManager.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
        break; // Salir si el fallo es definitivo
    }
    delay(100);
  }

  // Si salimos por timeout o fallo definitivo
  led.setState(ERROR_WIFI); // El LED se manejará después
  delay(3000);
  return false;
}

/**
 * Inicializa todos los sensores
 * @return true si todos los sensores iniciaron correctamente
 */
bool initializeSensors() {
  // Inicializar el sensor DHT22 (no usa I2C)
  dhtSensor.begin();
  
  // Inicializar un solo bus I2C para ambos sensores
  i2cBus.begin(I2C_SDA, I2C_SCL);
  i2cBus.setClock(100000); // 100 kHz, velocidad segura para la mayoría de sensores I2C
  delay(100);
  
  // Inicializar sensor de luz
  if (!lightSensor.begin()) {
    led.setState(ERROR_SENSOR);
    delay(3000);
    return false;
  }
  
  delay(100);
  
  // Inicializar cámara térmica
  if (!thermalSensor.begin()) {
    led.setState(ERROR_SENSOR);
    delay(3000);
    return false;
  }
  
  delay(500);
  
  // Inicializar cámara OV2640 al final
  if (!camera.begin()) {
    led.setState(ERROR_SENSOR);
    delay(3000);
    return false;
  }

  delay(500);
  return true;
}

/**
 * Lee datos de los sensores ambientales y los envía a la API.
 * @return true si la lectura y el envío fueron exitosos, false en caso contrario.
 */
bool readEnvironmentData() { 
  // Leer sensor de luz con reintentos
  float lightLevel = -1;
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    lightLevel = lightSensor.readLightLevel();
    if (lightLevel >= 0) break;
    delay(500);
  }
  
  // Leer sensor de temperatura y humedad con reintentos
  float temperature = NAN;
  float humidity = NAN;
  for (int i = 0; i < SENSOR_READ_RETRIES; i++) {
    temperature = dhtSensor.readTemperature();
    humidity = dhtSensor.readHumidity();
    if (!isnan(temperature) && !isnan(humidity)) break;
    delay(1000);
  }
  
  // Verificar si hubo errores de lectura
  if (lightLevel < 0 || isnan(temperature) || isnan(humidity)) {
    led.setState(ERROR_DATA); // Indicar error de sensor
    delay(3000);
    return false; // Retornar fallo para manejar en el loop principal
  }
  
  // Enviar los datos a la librería EnvironmentDataJSON para que arme y envíe el JSON
  bool sendSuccess = EnvironmentDataJSON::IOEnvironmentData(
    String(API_ENVIRONMENTAL), 
    lightLevel, 
    temperature, 
    humidity
  );
  led.setState(SENDING_DATA); // Indicar que se está enviando datos
  delay(3000);
  
  if (!sendSuccess) {
    led.setState(ERROR_SEND); // Indicar error de envío (más específico)
    delay(3000);
    return false; // Retornar fallo para manejar en el loop principal
  }

  return true; // Si llegamos aquí, todo fue bien
}

/**
 * Captura datos de las cámaras térmica y óptica
 * @param jpegImage Puntero donde se almacenará la dirección de la imagen JPEG
 * @param jpegLength Variable donde se almacenará el tamaño de la imagen
 * @param thermalData Puntero donde se almacenará la dirección de los datos térmicos
 * @return true si la captura fue exitosa
 */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  // Inicializar punteros a nullptr para evitar basura de memoria
  *jpegImage = nullptr;
  *thermalData = nullptr;
  jpegLength = 0;

  // Capturar imagen térmica
  if (!thermalSensor.readFrame()) {
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }
  
  float* rawThermalData = thermalSensor.getThermalData();
  if (rawThermalData == nullptr) {
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }
  
  // Usar heap_caps_malloc en lugar de ps_malloc para mayor compatibilidad
  *thermalData = (float*)heap_caps_malloc(32 * 24 * sizeof(float), MALLOC_CAP_8BIT);
  if (*thermalData == nullptr) {
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }
  memcpy(*thermalData, rawThermalData, 32 * 24 * sizeof(float));
  
  // Capturar imagen de la cámara con comprobación de memoria
  *jpegImage = camera.captureJPEG(jpegLength);
  if (*jpegImage == nullptr || jpegLength == 0) {
    // Liberar memoria asignada para evitar fugas
    heap_caps_free(*thermalData);
    *thermalData = nullptr;
    
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }
  
  return true;
}

/**
 * Envía datos de imágenes a la API.
 * @param jpegImage Puntero a la imagen JPEG
 * @param jpegLength Tamaño de la imagen
 * @param thermalData Puntero a los datos térmicos
 * @return true si el envío fue exitoso, false en caso contrario.
 */
// void sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData) { // Firma anterior
bool sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData) { 
  // Verificar que los punteros no sean nulos antes de enviar
  if (jpegImage == nullptr || thermalData == nullptr || jpegLength == 0) {
    led.setState(ERROR_DATA);
    delay(3000);
    return false;
  }
  
  // Usar MultipartDataSender para enviar datos térmicos e imagen
  bool sendSuccess = MultipartDataSender::IOThermalAndImageData(
    String(API_THERMAL), 
    thermalData, 
    jpegImage, 
    jpegLength
  );
  
  led.setState(SENDING_DATA); // Indicar que se está enviando datos
  delay(3000);

  if (!sendSuccess) {
    led.setState(ERROR_SEND); // Indicar error de envío
    delay(3000);
    return false; // Retornar fallo
  }
  return true;
}

/**
 * Limpia los recursos dinámicos
 */
void cleanupResources(uint8_t* jpegImage, float* thermalData) {
  // Liberar memoria de la imagen JPEG 
  if (jpegImage != nullptr) {
    heap_caps_free(jpegImage); 
    jpegImage = nullptr; 
  }
  
  // Liberar memoria de los datos térmicos 
  if (thermalData != nullptr) {
    heap_caps_free(thermalData); 
    thermalData = nullptr; 
  }
}

/**
 * Pone el ESP32 en modo de sueño profundo
 * @param seconds Tiempo de sueño en segundos
 */
void deepSleep(unsigned long seconds) {
  // Apagar el LED de estado
  led.turnOffAll();
  
  // Configurar el tiempo de sueño
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  
  // Entrar en sueño profundo
  esp_deep_sleep_start();
}