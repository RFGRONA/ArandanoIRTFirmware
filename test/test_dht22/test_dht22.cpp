#include <unity.h>
#include "DHT22Sensor.h"

// Instancia global del sensor para reutilizar en todas las pruebas
DHT22Sensor sensor(38); // Pin de datos del sensor DHT22

void test_dht22_sensor_initialization(void) {
    sensor.begin();
    TEST_ASSERT_FALSE_MESSAGE(sensor.hasError(), "Error en inicialización del DHT22");
}

void test_dht22_read_temperature(void) {
    float temperature = sensor.readTemperature();
    TEST_ASSERT_FALSE_MESSAGE(isnan(temperature), "Lectura de temperatura inválida");
    TEST_ASSERT_FALSE_MESSAGE(sensor.hasError(), "Error en lectura de temperatura");
}

void test_dht22_read_humidity(void) {
    float humidity = sensor.readHumidity();
    TEST_ASSERT_FALSE_MESSAGE(isnan(humidity), "Lectura de humedad inválida");
    TEST_ASSERT_FALSE_MESSAGE(sensor.hasError(), "Error en lectura de humedad");
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Estabilización para placas ESP

    UNITY_BEGIN();
    RUN_TEST(test_dht22_sensor_initialization);
    delay(100); // Tiempo para estabilizar el sensor
    
    RUN_TEST(test_dht22_read_temperature);
    RUN_TEST(test_dht22_read_humidity);
    UNITY_END(); // Finaliza todas las pruebas aquí
}

void loop() {
    // Vacío - Las pruebas solo se ejecutan una vez
}