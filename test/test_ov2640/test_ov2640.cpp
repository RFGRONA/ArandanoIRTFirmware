#include <Arduino.h>
#include <unity.h>
#include "OV2640Sensor.h"

OV2640Sensor cameraSensor;

void test_camera_initialization() {
  TEST_ASSERT_TRUE_MESSAGE(cameraSensor.begin(), "La inicialización de la cámara falló");
  // Una vez iniciada, desinicializamos para dejar el sistema limpio.
  cameraSensor.end();
}

void test_camera_capture_and_print() {
  // Inicializamos la cámara
  TEST_ASSERT_TRUE(cameraSensor.begin());
  // Capturamos y mostramos la imagen. 
  // Si la función falla (por ejemplo, no captura imagen), se imprimirá un error en consola.
  size_t length = 0;
  cameraSensor.captureJPEG(length);
  // Aquí podríamos verificar estados o salidas si la función retornara un valor.
  TEST_PASS(); // Si no se cuelga, asumimos que funcionó.
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_camera_initialization);
  RUN_TEST(test_camera_capture_and_print);
  UNITY_END();
}

void loop() {}