#include <Arduino.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include "DHT22Sensor.h"
#include "BH1750Sensor.h"
#include "MLX90640Sensor.h"
#include "OV2640Sensor.h"
#include "LEDStatus.h"

// Definición de pines
#define I2C_SDA 42
#define I2C_SCL 41
#define DHT_PIN 38

// Configuración general
#define SLEEP_DURATION_SECONDS 10  // 5 minutos de reposo
#define SENSOR_READ_RETRIES 3       // Intentos de lectura para sensores

TwoWire i2cBus = TwoWire(0);

// Instancias de los sensores
BH1750Sensor lightSensor(i2cBus, I2C_SDA, I2C_SCL);
MLX90640Sensor thermalSensor(i2cBus);
DHT22Sensor dhtSensor(DHT_PIN);
OV2640Sensor camera;
LEDStatus led;

// Variable para controlar errores
bool sensorsOK = true;

// Prototipos de funciones
bool initializeSensors();
void readEnvironmentData(JsonDocument& doc);
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData);
void sendEnvironmentData(JsonDocument& doc);
void sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData);
void printDebugInfo(JsonDocument& doc, uint8_t* jpegImage, size_t jpegLength, float* thermalData);
void deepSleep(unsigned long seconds);
void cleanupResources(uint8_t* jpegImage, float* thermalData);

void setup() {
  Serial.begin(115200);
  delay(500);  // Breve pausa para estabilizar
  
  // Iniciar el LED de estado
  led.begin();
  led.setState(TAKING_DATA);
  
  Serial.println("Iniciando sistema de monitoreo de plantas...");
  
  // Inicializar sensores
  if (!initializeSensors()) {
    // Si hay error al inicializar, dormimos y reiniciamos
    led.setState(ERROR_SENSOR);
    delay(5000);
    deepSleep(SLEEP_DURATION_SECONDS);
    return;
  }
  
  led.setState(ALL_OK);
  delay(1000);
}

void loop() {
  // Variables para almacenar datos
  JsonDocument envData;  // Documento JSON para datos ambientales
  uint8_t* jpegImage = nullptr;     // Puntero para la imagen JPEG
  size_t jpegLength = 0;            // Longitud de la imagen JPEG
  float* thermalData = nullptr;     // Copia del array térmico
  
  sensorsOK = true;  // Reiniciar flag de estado de sensores
  
  // 1. Indicar que estamos tomando datos
  led.setState(TAKING_DATA);
  delay(500);
  
  // 2. Leer datos ambientales (DHT22 y BH1750)
  readEnvironmentData(envData);
  
  // 3. Capturar imágenes (térmica y cámara)
  if (!captureImages(&jpegImage, jpegLength, &thermalData)) {
    sensorsOK = false;
    led.setState(ERROR_SENSOR);
    delay(2000);
  } else {
    led.setState(SENDING_DATA);
    
    // 4. Enviar datos ambientales
    sendEnvironmentData(envData);
    
    // 5. Enviar datos de imágenes
    sendImageData(jpegImage, jpegLength, thermalData);
  }
  
  // 6. Debug - Imprimir información
  printDebugInfo(envData, jpegImage, jpegLength, thermalData);
  
  // 7. Limpieza de recursos
  cleanupResources(jpegImage, thermalData);
  
  // 8. Indicar estado final
  if (sensorsOK) {
    led.setState(ALL_OK);
  } else {
    led.setState(ERROR_SENSOR);
  }
  
  delay(2000);
  
  // 9. Dormir y reiniciar
  deepSleep(SLEEP_DURATION_SECONDS);
}

/**
 * Inicializa todos los sensores
 * @return true si todos los sensores iniciaron correctamente
 */
bool initializeSensors() {
  Serial.println("Inicializando sensores...");
  
  // Inicializar el sensor DHT22 (no usa I2C)
  dhtSensor.begin();
  Serial.println("Sensor DHT22 inicializado");
  
  // Inicializar un solo bus I2C para ambos sensores
  i2cBus.begin(I2C_SDA, I2C_SCL);
  i2cBus.setClock(100000); // 100 kHz, velocidad segura para la mayoría de sensores I2C
  delay(100);
  
  // Inicializar sensor de luz
  if (!lightSensor.begin()) {
    Serial.println("Error al inicializar el sensor de luz BH1750");
    return false;
  }
  Serial.println("Sensor de luz BH1750 inicializado");
  
  delay(100);
  
  // Inicializar cámara térmica
  if (!thermalSensor.begin()) {
    Serial.println("Error al inicializar el sensor térmico MLX90640");
    return false;
  }
  Serial.println("Cámara térmica MLX90640 inicializada");
  
  delay(500);
  
  // Inicializar cámara OV2640 al final
  if (!camera.begin()) {
    Serial.println("Error al inicializar la cámara OV2640");
    return false;
  }
  Serial.println("Cámara OV2640 inicializada");
  
  return true;
}

/**
 * Lee datos de los sensores ambientales y los añade al documento JSON
 * @param doc Documento JSON donde se almacenarán los datos
 */
void readEnvironmentData(JsonDocument& doc) {
  Serial.println("Leyendo datos ambientales...");
  
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
  
  // Verificar si hubo errores
  if (lightLevel < 0 || isnan(temperature) || isnan(humidity)) {
    sensorsOK = false;
    led.setState(ERROR_SENSOR);
    delay(1000);
  }
  
  // Añadir datos al JSON
  doc["light"] = lightLevel;
  doc["temperature"] = temperature;
  doc["humidity"] = humidity;
  doc["timestamp"] = millis();
  
  Serial.println("Datos ambientales recopilados");
}

/**
 * Captura datos de las cámaras térmica y óptica
 * @param jpegImage Puntero donde se almacenará la dirección de la imagen JPEG
 * @param jpegLength Variable donde se almacenará el tamaño de la imagen
 * @param thermalData Puntero donde se almacenará la dirección de los datos térmicos
 * @return true si la captura fue exitosa
 */
bool captureImages(uint8_t** jpegImage, size_t& jpegLength, float** thermalData) {
  Serial.println("Capturando imágenes...");
  
  // Capturar imagen térmica
  if (!thermalSensor.readFrame()) {
    Serial.println("Error al capturar frame térmico");
    return false;
  }
  
  float* rawThermalData = thermalSensor.getThermalData();
  if (rawThermalData == nullptr) {
    Serial.println("Error: datos térmicos no disponibles");
    return false;
  }
  
  // Crear una copia de los datos térmicos
  *thermalData = (float*)malloc(32 * 24 * sizeof(float));
  if (*thermalData == nullptr) {
    Serial.println("Error: no se pudo asignar memoria para datos térmicos");
    return false;
  }
  memcpy(*thermalData, rawThermalData, 32 * 24 * sizeof(float));
  
  // Capturar imagen de la cámara
  *jpegImage = camera.captureJPEG(jpegLength);
  if (*jpegImage == nullptr || jpegLength == 0) {
    Serial.println("Error al capturar imagen JPEG");
    
    // Liberar memoria asignada para evitar fugas
    if (*thermalData != nullptr) {
      free(*thermalData);
      *thermalData = nullptr;
    }
    
    return false;
  }
  
  Serial.printf("Imágenes capturadas con éxito. JPEG: %u bytes\n", jpegLength);
  return true;
}

/**
 * Envía datos ambientales a la API
 * @param doc Documento JSON con los datos a enviar
 */
void sendEnvironmentData(JsonDocument& doc) {
  Serial.println("Enviando datos ambientales a la API...");
  // Método a implementar en el futuro
  delay(1000); // Simulación de envío
}

/**
 * Envía datos de imágenes a la API
 * @param jpegImage Puntero a la imagen JPEG
 * @param jpegLength Tamaño de la imagen
 * @param thermalData Puntero a los datos térmicos
 */
void sendImageData(uint8_t* jpegImage, size_t jpegLength, float* thermalData) {
  Serial.println("Enviando imágenes a la API...");
  // Método a implementar en el futuro
  delay(1000); // Simulación de envío
}

/**
 * Imprime información de depuración
 */
void printDebugInfo(JsonDocument& doc, uint8_t* jpegImage, size_t jpegLength, float* thermalData) {
  Serial.println("\n--- INFORMACIÓN DE DEPURACIÓN ---");
  Serial.println("Datos ambientales:");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  Serial.printf("Imagen JPEG: %u bytes\n", jpegLength);
  Serial.print("Primeros datos de la imagen JPEG: ");
  size_t numBytesToPrint = (jpegLength < 10) ? jpegLength : 10;
  for (size_t i = 0; i < numBytesToPrint; i++) {
    Serial.printf("0x%02X ", jpegImage[i]);
  }
  Serial.println();
  
  if (thermalData != nullptr) {
    float sum = 0.0;
    float minTemp = thermalData[0];
    float maxTemp = thermalData[0];
    // Suponiendo que la matriz térmica es de 32x24 (768 elementos)
    for (int i = 0; i < 32 * 24; i++) {
      float temp = thermalData[i];
      sum += temp;
      if (temp < minTemp) minTemp = temp;
      if (temp > maxTemp) maxTemp = temp;
    }
    float avgTemp = sum / (32 * 24);
    Serial.printf("Temperatura promedio: %.2f °C\n", avgTemp);
    Serial.printf("Temperatura mínima: %.2f °C\n", minTemp);
    Serial.printf("Temperatura máxima: %.2f °C\n", maxTemp);
  }
  
  Serial.println("--- FIN DE LA INFORMACIÓN DE DEPURACIÓN ---\n");
}

/**
 * Limpia los recursos dinámicos
 */
void cleanupResources(uint8_t* jpegImage, float* thermalData) {
  // Liberar memoria de la imagen JPEG
  if (jpegImage != nullptr) {
    free(jpegImage);
  }
  
  // Liberar memoria de los datos térmicos
  if (thermalData != nullptr) {
    free(thermalData);
  }
}

/**
 * Pone el ESP32 en modo de sueño profundo
 * @param seconds Tiempo de sueño en segundos
 */
void deepSleep(unsigned long seconds) {
  Serial.printf("Entrando en modo de sueño profundo por %lu segundos...\n", seconds);
  
  // Apagar el LED de estado
  led.turnOffAll();
  
  // Esperar a que se terminen de imprimir los mensajes
  Serial.flush();
  
  // Configurar el tiempo de sueño
  esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
  
  // Entrar en sueño profundo
  esp_deep_sleep_start();
}