#include <Arduino.h>
#include <unity.h>
#include "MLX90640Sensor.h"

// Crear una instancia de TwoWire para el test (bus 1, por ejemplo)
TwoWire testWire(1);
MLX90640Sensor sensor(testWire); // Se pasa el bus de comunicación

void test_mlx90640_sensor_initialization(void) {
    // Inicializar el bus I2C para el MLX90640
    testWire.begin(47, 21); // SDA y SCL
    bool initialized = sensor.begin();
    TEST_ASSERT_TRUE_MESSAGE(initialized, "Falló la inicialización del sensor MLX90640");
}

void test_mlx90640_read_frame(void) {
    bool frameRead = sensor.readFrame();
    TEST_ASSERT_TRUE_MESSAGE(frameRead, "Falló la lectura del frame del sensor MLX90640");
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    RUN_TEST(test_mlx90640_sensor_initialization);
    RUN_TEST(test_mlx90640_read_frame);
    UNITY_END();
}

void loop() {}