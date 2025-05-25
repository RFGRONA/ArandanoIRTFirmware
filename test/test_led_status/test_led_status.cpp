// Include necessary libraries
#include <Arduino.h>    // Arduino core framework
#include <unity.h>      // Unity test framework
#include "LEDStatus.h"  // Custom LEDStatus class header (ensure path is correct)

// Create an instance of the LEDStatus class for testing
LEDStatus testLed;

// Define the time (in milliseconds) to visually observe each LED state
#define STATE_VISUAL_DELAY 1000

// setUp function: runs before each test (currently empty)
void setUp(void) {}

// tearDown function: runs after each individual test
void tearDown(void) {
  // Turn off all LEDs after each test to start the next one cleanly
  testLed.turnOffAll();
  // Short pause
  delay(50);
}

// --- Individual test functions for each LED state ---
// These tests primarily serve to visually confirm each state works.

// Test setting the LED state to ALL_OK
void test_led_state_all_ok(void) {
    testLed.setState(ALL_OK);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to ERROR_AUTH
void test_led_state_error_auth(void) {
    testLed.setState(ERROR_AUTH);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to ERROR_SEND
void test_led_state_error_send(void) {
    testLed.setState(ERROR_SEND);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to ERROR_SENSOR
void test_led_state_error_sensor(void) {
    testLed.setState(ERROR_SENSOR);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to ERROR_DATA
void test_led_state_error_data(void) {
    testLed.setState(ERROR_DATA);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to TAKING_DATA
void test_led_state_taking_data(void) {
    testLed.setState(TAKING_DATA);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to SENDING_DATA
void test_led_state_sending_data(void) {
    testLed.setState(SENDING_DATA);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to CONNECTING_WIFI
void test_led_state_connecting_wifi(void) {
    testLed.setState(CONNECTING_WIFI);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// Test setting the LED state to ERROR_WIFI
void test_led_state_error_wifi(void) {
    testLed.setState(ERROR_WIFI);
    delay(STATE_VISUAL_DELAY); // Wait for visual confirmation
}

// --- Setup function: executes all tests ---
void setup() {
    // Initial delay
    delay(2000);

    // Initialize the LED controller once at the beginning
    testLed.begin();

    // Begin the Unity test framework
    UNITY_BEGIN();

    // Run one test for each defined state
    RUN_TEST(test_led_state_all_ok);
    RUN_TEST(test_led_state_error_auth);
    RUN_TEST(test_led_state_error_send);
    RUN_TEST(test_led_state_error_sensor);
    RUN_TEST(test_led_state_error_data);
    RUN_TEST(test_led_state_taking_data);
    RUN_TEST(test_led_state_sending_data);
    RUN_TEST(test_led_state_connecting_wifi);
    RUN_TEST(test_led_state_error_wifi);

    // End the Unity test framework and report results
    UNITY_END();
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {
    // Empty
}