#include <Arduino.h>
#include <unity.h>
#include "MLX90640Sensor.h" 
#include <Wire.h>

#define SDA_PIN 42 
#define SCL_PIN 41

// Usar el mismo bus que el código principal
TwoWire testWire(0);
MLX90640Sensor testMlxSensor(testWire);

// Variable global para verificar inicialización
bool mlxInitialized = false;

void setUp(void) {}

void tearDown(void) {}

// Test de inicialización (solo se llama si begin() en setup tuvo éxito)
void test_mlx90640_reinitialization_check() {
    TEST_ASSERT_TRUE_MESSAGE(mlxInitialized, "La inicialización global del MLX90640 falló");
}

// Test de lectura de frame y obtención de datos
void test_mlx90640_read_frame_and_get_data() {
    TEST_ASSERT_TRUE_MESSAGE(mlxInitialized, "Skipping test: MLX90640 no inicializado");
    bool frameRead = testMlxSensor.readFrame();
    TEST_ASSERT_TRUE_MESSAGE(frameRead, "Falló la lectura del frame del MLX90640");

    // Verificar si podemos obtener el puntero a los datos
    if (frameRead) {
        float* thermalData = testMlxSensor.getThermalData();
        TEST_ASSERT_NOT_NULL_MESSAGE(thermalData, "getThermalData() retornó NULL después de leer frame");
    }
}

void setup() {
    // Serial.begin(115200);
    delay(2000);

    // Inicializar I2C UNA VEZ
    testWire.begin(SDA_PIN, SCL_PIN);
    testWire.setClock(400000); 
    delay(100);

    // Intentar inicializar el sensor y guardar estado
    mlxInitialized = testMlxSensor.begin();
    delay(500); // Dar tiempo al sensor

    UNITY_BEGIN();
    RUN_TEST(test_mlx90640_reinitialization_check); 
    RUN_TEST(test_mlx90640_read_frame_and_get_data);
    UNITY_END();
}

void loop() {}