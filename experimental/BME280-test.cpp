// experimental/BME280-test.cpp
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// --- Configuración de Pines I2C ---
// Asegúrate de que coincidan con los pines de tu placa
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 21

Adafruit_BME280 bme; // Objeto para el sensor BME280

void setup() {
  Serial.begin(115200);
  while (!Serial); // Espera a que el puerto serie se conecte

  Serial.println("\n--- BME280 Test ---");

  // Inicializa el bus I2C con los pines definidos
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Inicializa el sensor BME280
  // La dirección I2C por defecto suele ser 0x77 o 0x76.
  // La librería de Adafruit la detecta automáticamente.
  if (!bme.begin(0x76)) {
    Serial.println("¡No se pudo encontrar el sensor BME280! Revisa el cableado.");
    while (1) delay(10); // Detiene la ejecución si el sensor no se encuentra
  }

  Serial.println("Sensor BME280 inicializado correctamente.");
}

void loop() {
  Serial.print("Temperatura = ");
  Serial.print(bme.readTemperature());
  Serial.println(" °C");

  Serial.print("Presión = ");
  Serial.print(bme.readPressure() / 100.0F); // hPa
  Serial.println(" hPa");

  Serial.print("Humedad = ");
  Serial.print(bme.readHumidity());
  Serial.println(" %");

  Serial.println();
  delay(2000); // Espera 2 segundos entre lecturas
}