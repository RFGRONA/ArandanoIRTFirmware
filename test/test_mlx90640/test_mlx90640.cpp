// Include necessary libraries
#include <Arduino.h>         // Arduino core framework
#include <unity.h>           // Unity test framework
#include "MLX90640Sensor.h"  // Custom MLX90640 sensor class header
#include <Wire.h>            // I2C communication library

// Define I2C pins
#define SDA_PIN 47 // I2C Data pin
#define SCL_PIN 21 // I2C Clock pin

// Use the same I2C bus as the main code
// Initialize the I2C bus for testing (using bus 0)
TwoWire testWire(0);
// Create an instance of the MLX90640 sensor for testing, passing the I2C bus
MLX90640Sensor testMlxSensor(testWire);

// Global flag to track if the sensor initialization in setup() was successful
bool mlxInitialized = false;

// setUp function: runs before each test (currently empty)
void setUp(void) {}
// tearDown function: runs after each test (currently empty)
void tearDown(void) {}

// Test function to check the result of the initial setup initialization
// This test only runs if the begin() call in setup() was successful
void test_mlx90640_reinitialization_check() {
    // Assert that the global initialization flag is true
    TEST_ASSERT_TRUE_MESSAGE(mlxInitialized, "Global MLX90640 initialization failed");
}

// Test function for reading a frame and getting thermal data
void test_mlx90640_read_frame_and_get_data() {
    // First, ensure the sensor was initialized correctly in setup()
    TEST_ASSERT_TRUE_MESSAGE(mlxInitialized, "Skipping test: MLX90640 not initialized");

    // Attempt to read a frame from the sensor
    bool frameRead = testMlxSensor.readFrame();
    // Assert that the frame read was successful
    TEST_ASSERT_TRUE_MESSAGE(frameRead, "Failed to read frame from MLX90640");

    // If the frame was read successfully, try to get the data pointer
    if (frameRead) {
        // Get the pointer to the thermal data array
        float* thermalData = testMlxSensor.getThermalData();
        // Assert that the returned pointer is not NULL
        TEST_ASSERT_NOT_NULL_MESSAGE(thermalData, "getThermalData() returned NULL after successful frame read");
    }
}

// Setup function: runs once at the beginning
void setup() {
    // Optional: Start serial communication for debugging
    // Serial.begin(115200);
    // Initial delay for stability
    delay(2000);

    // Initialize I2C communication ONCE
    testWire.begin(SDA_PIN, SCL_PIN);
    // Set the I2C clock speed (MLX90640 often requires 400kHz or more)
    testWire.setClock(400000);
    // Short delay after I2C setup
    delay(100);

    // Attempt to initialize the sensor and store the result in the global flag
    mlxInitialized = testMlxSensor.begin();
    // Allow some time for the sensor to initialize internally
    delay(500);

    // Begin the Unity test framework
    UNITY_BEGIN();
    // Run the test to check the initialization status flag
    RUN_TEST(test_mlx90640_reinitialization_check);
    // Run the test to read a frame and get data
    RUN_TEST(test_mlx90640_read_frame_and_get_data);
    // End the Unity test framework and report results
    UNITY_END();
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {}