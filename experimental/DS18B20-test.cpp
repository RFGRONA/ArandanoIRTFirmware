// experimental/DS18B20-test.cpp
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Configuración de Pin ---
// ¡IMPORTANTE! Cambia este pin al GPIO donde conectaste el sensor DS18B20
#define ONE_WIRE_BUS_PIN 41

// Configura una instancia de OneWire para comunicarse con cualquier dispositivo OneWire
OneWire oneWire(ONE_WIRE_BUS_PIN);

// Pasa la referencia de oneWire a la librería DallasTemperature
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("\n--- DS18B20 Test ---");
  
  // Inicia la librería DallasTemperature
  sensors.begin();
  Serial.println("Librería DallasTemperature iniciada.");
}

void loop() {
  Serial.print("Solicitando temperaturas...");
  sensors.requestTemperatures(); // Envía el comando para obtener las temperaturas
  Serial.println(" ¡Hecho!");

  // Obtenemos la temperatura del primer sensor en el bus (índice 0)
  float tempC = sensors.getTempCByIndex(0);

  // Comprueba si la lectura fue válida
  if (tempC == DEVICE_DISCONNECTED_C) {
    Serial.println("Error: No se pudo leer la temperatura del sensor.");
  } else {
    Serial.print("Temperatura: ");
    Serial.print(tempC);
    Serial.println(" °C");
  }

  Serial.println();
  delay(2000); // Espera 2 segundos entre lecturas
}