#include <Arduino.h> 
#include <unity.h>
#include "DHT22Sensor.h" 

#define DHT_TEST_PIN 47 

DHT22Sensor testDhtSensor(DHT_TEST_PIN);


void setUp(void) {}
void tearDown(void) {}

// Test de inicialización (solo verifica que begin() no cause problemas)
void test_dht22_initialization(void) {
    testDhtSensor.begin();
    delay(100); // Pequeña pausa tras iniciar
}

// Test de lectura de temperatura
void test_dht22_read_temperature(void) {
    delay(2100); 
    float temperature = testDhtSensor.readTemperature();
    // La prueba falla si la lectura es NaN (Not a Number)
    TEST_ASSERT_FALSE_MESSAGE(isnan(temperature), "Lectura de temperatura retornó NaN");
}

// Test de lectura de humedad
void test_dht22_read_humidity(void) {
    delay(2100); 
    float humidity = testDhtSensor.readHumidity();
    TEST_ASSERT_FALSE_MESSAGE(isnan(humidity), "Lectura de humedad retornó NaN");
}

// setup() se ejecuta una vez al inicio
void setup() {
    // Serial.begin(115200);
    delay(2000); // Delay inicial para estabilización

    // Inicializa el sensor UNA VEZ antes de correr los tests
    testDhtSensor.begin();
    // Esperar a que el sensor se estabilice tras el begin() inicial
    delay(2100); // ¡IMPORTANTE! Esperar >2 segundos entre lecturas DHT

    UNITY_BEGIN();
    RUN_TEST(test_dht22_read_temperature);
    RUN_TEST(test_dht22_read_humidity);
    UNITY_END();
}

void loop() {}