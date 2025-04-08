// Include necessary libraries
#include <Arduino.h>      // Arduino core framework
#include <unity.h>        // Unity test framework
#include "DHT22Sensor.h"  // Custom DHT22 sensor class header

// Define the pin connected to the DHT22 sensor's data line
#define DHT_TEST_PIN 14

// Create an instance of the DHT22 sensor for testing
DHT22Sensor testDhtSensor(DHT_TEST_PIN);

// setUp function: runs before each test (currently empty)
void setUp(void) {}
// tearDown function: runs after each test (currently empty)
void tearDown(void) {}

// Test function for sensor initialization
// (This mainly checks that calling begin() doesn't cause issues)
void test_dht22_initialization(void) {
    // Initialize the sensor
    testDhtSensor.begin();
    // Short delay after initialization
    delay(100);
}

// Test function for reading temperature
void test_dht22_read_temperature(void) {
    // Wait for the required time between DHT readings (>2 seconds)
    delay(2100);
    // Read the temperature
    float temperature = testDhtSensor.readTemperature();
    // Assert that the reading is not NaN (Not a Number), which indicates a read failure
    TEST_ASSERT_FALSE_MESSAGE(isnan(temperature), "Temperature reading returned NaN");
}

// Test function for reading humidity
void test_dht22_read_humidity(void) {
    // Wait for the required time between DHT readings (>2 seconds)
    delay(2100);
    // Read the humidity
    float humidity = testDhtSensor.readHumidity();
    // Assert that the reading is not NaN (Not a Number), which indicates a read failure
    TEST_ASSERT_FALSE_MESSAGE(isnan(humidity), "Humidity reading returned NaN");
}

// Setup function: runs once at the beginning
void setup() {
    // Optional: Start serial communication for debugging
    // Serial.begin(115200);
    // Initial delay for stabilization
    delay(2000);

    // Initialize the sensor ONCE before running any tests
    testDhtSensor.begin();
    // Wait for the sensor to stabilize after the initial begin() call
    // IMPORTANT! DHT sensors require a >2 second delay between readings
    delay(2100);

    // Begin the Unity test framework
    UNITY_BEGIN();
    // Run the temperature reading test
    RUN_TEST(test_dht22_read_temperature);
    // Run the humidity reading test
    RUN_TEST(test_dht22_read_humidity);
    // End the Unity test framework and report results
    UNITY_END();
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {}