// Include necessary libraries
#include <Arduino.h>      // Arduino core framework
#include <unity.h>        // Unity test framework
#include "BH1750Sensor.h" // Custom BH1750 sensor class header
#include <Wire.h>         // I2C communication library

// Define I2C pins
#define SDA_PIN 47 // I2C Data pin
#define SCL_PIN 21 // I2C Clock pin

// Use the same I2C bus as the main code if testing on the same hardware
// Initialize the I2C bus for testing (using bus 0)
TwoWire testWire(0);
// Create an instance of the BH1750 sensor for testing, passing the I2C bus and pins
BH1750Sensor testBhSensor(testWire, SDA_PIN, SCL_PIN);

// Test function for sensor initialization
void test_bh1750_initialization() {
   // Attempt to initialize the sensor
   bool success = testBhSensor.begin();
   // Assert that the initialization was successful
   TEST_ASSERT_TRUE_MESSAGE(success, "BH1750 begin() failed");
}

// Test function for reading light levels (more robust)
void test_bh1750_light_reading() {
  // Perform multiple readings to check for consistency
  for (int i = 0; i < 3; ++i) {
    // Read the light level from the sensor
    float lux = testBhSensor.readLightLevel();
    // Assert that the lux reading is not negative (which would indicate an error)
    TEST_ASSERT_FALSE_MESSAGE(lux < 0, "Negative Lux reading, indicates error");
    // Assert that the lux reading is not excessively high (within expected range)
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(130000.0, lux, "Excessive Lux reading");
    // Wait a short period between readings
    delay(500);
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
  // Set the I2C clock speed (same as in main.ino)
  testWire.setClock(100000);
  // Short delay after I2C setup
  delay(100);

  // Attempt to initialize the sensor before running tests
  bool initialized = testBhSensor.begin();

  // Begin the Unity test framework
  UNITY_BEGIN();

  // Only run tests if the sensor initialization in setup was successful
  if (initialized) {
      // Run the initialization test (re-verifies begin())
      RUN_TEST(test_bh1750_initialization);
      // Run the light reading test
      RUN_TEST(test_bh1750_light_reading);
  } else {
      // Mark a test as failed if the initial begin() in setup already failed
      TEST_FAIL_MESSAGE("Initial BH1750 begin() failed in setup()");
  }
  // End the Unity test framework and report results
  UNITY_END();
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {}