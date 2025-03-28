#include <Arduino.h>
#include <unity.h>
#include "BH1750Sensor.h" 
#include <Wire.h> 

#define SDA_PIN 42
#define SCL_PIN 41

// Usar el mismo bus que el código principal si se testea en el mismo hardware
TwoWire testWire(0);
BH1750Sensor testBhSensor(testWire, SDA_PIN, SCL_PIN);

// Test de inicialización
void test_bh1750_initialization() {
   bool success = testBhSensor.begin(); 
   TEST_ASSERT_TRUE_MESSAGE(success, "BH1750 begin() falló");
}

// Test de lectura de luz (más robusto)
void test_bh1750_light_reading() {
  // Realizar varias lecturas para verificar consistencia
  for (int i = 0; i < 3; ++i) {
    float lux = testBhSensor.readLightLevel();
    TEST_ASSERT_FALSE_MESSAGE(lux < 0, "Lectura de Lux negativa, indica error");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(130000.0, lux, "Lectura de Lux excesiva");
    delay(500); // Esperar un poco entre lecturas
  }
}

void setup() {
  // Serial.begin(115200);
  delay(2000);

  // Inicializar I2C UNA VEZ
  testWire.begin(SDA_PIN, SCL_PIN);
  testWire.setClock(100000); // Configurar reloj como en main.ino
  delay(100);

  // Intentar inicializar el sensor
  bool initialized = testBhSensor.begin();

  UNITY_BEGIN();
  // Solo ejecutar tests si la inicialización fue exitosa
  if (initialized) {
      RUN_TEST(test_bh1750_initialization); // Re-verifica begin()
      RUN_TEST(test_bh1750_light_reading);
  } else {
      // Marcar un test como fallido si begin() ya falló aquí
      TEST_FAIL_MESSAGE("Fallo inicial de BH1750 begin() en setup()");
  }
  UNITY_END();
}

void loop() {}