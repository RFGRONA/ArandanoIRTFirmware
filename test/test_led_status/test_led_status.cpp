#include <Arduino.h>
#include <unity.h>
#include "LEDStatus.h" // Asegúrate que la ruta sea correcta

LEDStatus testLed;

// Tiempo para visualizar cada estado en milisegundos
#define STATE_VISUAL_DELAY 1000 

void setUp(void) {}

void tearDown(void) {
  // Apagar el LED después de cada test individual para empezar limpio el siguiente
  testLed.turnOffAll();
  delay(50); // Pequeña pausa
}

// --- Tests individuales para cada estado ---

void test_led_state_all_ok(void) {
    testLed.setState(ALL_OK);
    delay(STATE_VISUAL_DELAY); 
}

void test_led_state_error_auth(void) {
    testLed.setState(ERROR_AUTH);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_error_send(void) {
    testLed.setState(ERROR_SEND);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_error_sensor(void) {
    testLed.setState(ERROR_SENSOR);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_error_data(void) {
    testLed.setState(ERROR_DATA);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_taking_data(void) {
    testLed.setState(TAKING_DATA);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_sending_data(void) {
    testLed.setState(SENDING_DATA);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_connecting_wifi(void) {
    testLed.setState(CONNECTING_WIFI);
    delay(STATE_VISUAL_DELAY);
}

void test_led_state_error_wifi(void) {
    testLed.setState(ERROR_WIFI);
    delay(STATE_VISUAL_DELAY);
}

// --- Función setup() que ejecuta todos los tests ---

void setup() {
    delay(2000); // Espera inicial
    
    // Inicializar el LED una vez al principio
    testLed.begin();

    UNITY_BEGIN();
    
    // Ejecutar un test para cada estado definido
    RUN_TEST(test_led_state_all_ok);
    RUN_TEST(test_led_state_error_auth);
    RUN_TEST(test_led_state_error_send);
    RUN_TEST(test_led_state_error_sensor);
    RUN_TEST(test_led_state_error_data);
    RUN_TEST(test_led_state_taking_data);
    RUN_TEST(test_led_state_sending_data);
    RUN_TEST(test_led_state_connecting_wifi);
    RUN_TEST(test_led_state_error_wifi);
    
    UNITY_END();
}

void loop() {
    // Vacío
}