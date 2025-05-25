// Include necessary libraries
#include <Arduino.h>        // Arduino core framework
#include <unity.h>          // Unity test framework
#include <esp_heap_caps.h>  // ESP32 heap capabilities functions (for PSRAM checks and allocation)

// setUp function: runs before each test (currently empty)
void setUp(void) {}
// tearDown function: runs after each test (currently empty)
void tearDown(void) {}

// Test function to verify if PSRAM is available and enabled
void test_psram_is_available() {
    // Use the ESP-IDF function psramFound() to check for PSRAM
    // Assert that PSRAM was found (returns true if present and enabled)
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "PSRAM not found or not enabled in configuration");
}

// Test function to verify the reported size of the PSRAM
void test_psram_size() {
    // Skip this test if PSRAM is not available
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "Skipping test: PSRAM not found");

    // Get the total size of the PSRAM in bytes
    size_t psramSize = ESP.getPsramSize();
    // We expect 8MB for an ESP32-WROVER with 16MB Flash / 8MB PSRAM (N16R8),
    // but we check for a reasonable minimum (>4MB) as the exact usable size might vary slightly.
    // Calculate 4MB in bytes
    const size_t minExpectedSize = 4 * 1024 * 1024;
    // Assert that the reported PSRAM size is greater than the minimum expected size
    TEST_ASSERT_GREATER_THAN_UINT32_MESSAGE(minExpectedSize, psramSize, "PSRAM size unexpectedly low");
}

// Test function to allocate and free memory specifically in PSRAM
void test_psram_allocation_and_free() {
    // Skip this test if PSRAM is not available
    TEST_ASSERT_TRUE_MESSAGE(psramFound(), "Skipping test: PSRAM not found");

    // Define the size of memory to allocate (e.g., 100KB)
    size_t allocSize = 1024 * 100;
    // Pointer to hold the allocated memory address
    void* psramPtr = nullptr;

    // Use heap_caps_malloc with MALLOC_CAP_SPIRAM to ensure allocation happens in PSRAM
    psramPtr = heap_caps_malloc(allocSize, MALLOC_CAP_SPIRAM);

    // Assert that the allocation was successful (pointer is not NULL)
    TEST_ASSERT_NOT_NULL_MESSAGE(psramPtr, "heap_caps_malloc failed to allocate in PSRAM");

    // If allocation was successful, perform a simple write/read test and free the memory
    if (psramPtr != nullptr) {
        // Simple write/read test: fill the allocated memory with a pattern
        memset(psramPtr, 0xA5, allocSize); // Fill with pattern 0xA5
        // Read the first byte and assert it matches the written pattern
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0xA5, *((uint8_t*)psramPtr), "Failed read/write test on allocated PSRAM memory");

        // Free the allocated PSRAM memory
        heap_caps_free(psramPtr);

        // Optional: Test allocation/free using ps_malloc if used elsewhere in your code
        // ps_malloc automatically tries to allocate in PSRAM if available.
        void* psPtr2 = ps_malloc(1024); // Allocate 1KB using ps_malloc
        TEST_ASSERT_NOT_NULL(psPtr2);   // Assert allocation was successful
        if (psPtr2) {
            free(psPtr2); // Free the memory allocated by ps_malloc using standard free()
        }
    }
}

// Setup function: runs once at the beginning
void setup() {
    // Initial delay
    delay(2000);
    // Begin the Unity test framework
    UNITY_BEGIN();
    // Run the PSRAM availability test
    RUN_TEST(test_psram_is_available);
    // Run the PSRAM size test
    RUN_TEST(test_psram_size);
    // Run the PSRAM allocation/free test
    RUN_TEST(test_psram_allocation_and_free);
    // End the Unity test framework and report results
    UNITY_END();
}

// Loop function: runs repeatedly after setup (empty for tests)
void loop() {}