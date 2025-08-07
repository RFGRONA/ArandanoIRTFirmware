// Include necessary libraries
#include <Arduino.h>       // Arduino core framework
#include <unity.h>         // Unity test framework
#include "OV2640Sensor.h"  // Custom OV2640 camera sensor class header
#include "esp_heap_caps.h" // ESP32 heap capabilities functions (for PSRAM allocation/free)

// Create an instance of the OV2640 camera sensor for testing
OV2640Sensor testCameraSensor;

// Global flag to track if the camera initialization in setup() was successful
bool cameraInitialized = false;

// setUp function: runs before each test (currently empty)
void setUp(void) {}
// tearDown function: runs after each test (currently empty)
void tearDown(void) {}

// Test function to check the status of the camera initialization performed in setup()
void test_camera_initialization_status() {
    // Assert that the global initialization flag is true
    TEST_ASSERT_TRUE_MESSAGE(cameraInitialized, "Camera initialization in setup() failed");
}

// Test function for capturing a JPEG image and verifying the buffer
void test_camera_capture_jpeg() {
    // First, ensure the camera was initialized correctly in setup()
    TEST_ASSERT_TRUE_MESSAGE(cameraInitialized, "Skipping test: Camera not initialized");

    // Variables to store the buffer pointer and length
    size_t length = 0;
    uint8_t* jpegBuffer = nullptr;

    // Attempt to capture a JPEG image
    jpegBuffer = testCameraSensor.captureJPEG(length);

    // Verify the results of the capture attempt
    // Assert that the returned buffer pointer is not NULL
    TEST_ASSERT_NOT_NULL_MESSAGE(jpegBuffer, "captureJPEG returned NULL");
    // Assert that the returned length is greater than 0
    TEST_ASSERT_GREATER_THAN_MESSAGE(0, length, "captureJPEG returned zero length");

    // If the capture was successful (buffer is not NULL), free the allocated memory
    // IMPORTANT: The captureJPEG function likely allocates memory (e.g., in PSRAM)
    // that needs to be freed by the caller.
    if (jpegBuffer != nullptr) {
        // Free the memory using the appropriate function (heap_caps_free for PSRAM)
        heap_caps_free(jpegBuffer);
    }
}

// Setup function: runs once at the beginning
void setup() {
    // Initial delay
    delay(2000);

    // Attempt to initialize the camera sensor ONCE and store the result
    cameraInitialized = testCameraSensor.begin();
    // Give the camera some time to initialize
    delay(500);

    // Begin the Unity test framework
    UNITY_BEGIN();
    // Run the test to check the initialization status flag (verifies setup worked)
    RUN_TEST(test_camera_initialization_status);
    // Run the test to capture a JPEG image
    RUN_TEST(test_camera_capture_jpeg);
    // End the Unity test framework and report results
    UNITY_END();

    // Deinitialize the camera after all tests are complete to release resources
    if (cameraInitialized) {
        testCameraSensor.end();
    }
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {}