#include <Arduino.h>
#include <unity.h>
#include "OV2640Sensor.h" 
#include "esp_heap_caps.h" 

OV2640Sensor testCameraSensor;

// Flag global para estado de inicialización
bool cameraInitialized = false;

void setUp(void) {}
void tearDown(void) {}

// Test de inicialización (solo verifica el flag)
void test_camera_initialization_status() {
    TEST_ASSERT_TRUE_MESSAGE(cameraInitialized, "La inicialización de la cámara en setup() falló");
}

// Test de captura y verificación del buffer
void test_camera_capture_jpeg() {
    TEST_ASSERT_TRUE_MESSAGE(cameraInitialized, "Skipping test: Cámara no inicializada");

    size_t length = 0;
    uint8_t* jpegBuffer = nullptr;

    // Intentar capturar
    jpegBuffer = testCameraSensor.captureJPEG(length);

    // Verificar resultados
    TEST_ASSERT_NOT_NULL_MESSAGE(jpegBuffer, "captureJPEG retornó NULL");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, length, "captureJPEG retornó longitud 0");

    // Si la captura fue exitosa, liberar el buffer
    if (jpegBuffer != nullptr) {
        heap_caps_free(jpegBuffer); 
    }
}

void setup() {
    delay(2000); // Espera inicial

    // Intentar inicializar la cámara UNA VEZ
    cameraInitialized = testCameraSensor.begin();
    delay(500); // Dar tiempo a la cámara

    UNITY_BEGIN();
    RUN_TEST(test_camera_initialization_status); // Verifica si el setup funcionó
    RUN_TEST(test_camera_capture_jpeg);
    UNITY_END();

    // Desinicializar la cámara después de las pruebas
    if (cameraInitialized) {
        testCameraSensor.end();
    }
}

void loop() {}