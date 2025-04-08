#include <Arduino.h>
#include <unity.h>       // Unity test framework
#include <Preferences.h> // Preferences library for NVS access

// Create a Preferences instance for testing
Preferences testPreferences;

// Define a namespace and keys specifically for testing
const char* NVS_TEST_NAMESPACE = "unity_nvs_test";
const char* KEY_UINT = "test_uint";
const char* KEY_INT = "test_int";
const char* KEY_STR = "test_str";
const char* KEY_BOOL = "test_bool";
const char* KEY_NONEXISTENT = "nonexistent"; // A key guaranteed not to be set initially

// --- Test Setup and Teardown ---

// Function runs before each test
void setUp(void) {
    // Ensure the namespace is clean before each test runs.
    // Open in read/write mode.
    bool opened = testPreferences.begin(NVS_TEST_NAMESPACE, false);
    if (opened) {
        testPreferences.clear(); // Clear all keys in the namespace
        testPreferences.end();   // Close the namespace
    } else {
        // If we can't even open to clear, fail fast.
        TEST_FAIL_MESSAGE("Failed to open NVS namespace in setUp()");
    }
    // Short delay to ensure NVS operations complete
    delay(50);
}

// Function runs after each test (optional, could also do cleanup here)
void tearDown(void) {
    // testPreferences.end(); // Might be useful if tests leave namespace open
}

// --- Test Cases ---

// Test if we can successfully open (begin) the NVS namespace
void test_nvs_begin(void) {
    bool success = testPreferences.begin(NVS_TEST_NAMESPACE, false); // Read/write mode
    TEST_ASSERT_TRUE_MESSAGE(success, "testPreferences.begin() failed");
    if (success) {
        testPreferences.end(); // Close it immediately after check
    }
}

// Test writing and then reading back different data types
void test_nvs_write_read_types(void) {
    // Values to write
    unsigned int testUIntVal = 12345;
    int testIntVal = -6789;
    String testStrVal = "Hello Unity!";
    bool testBoolVal = true;

    // 1. Open namespace for writing
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, false));

    // 2. Write values
    TEST_ASSERT_GREATER_THAN_UINT32(0, testPreferences.putUInt(KEY_UINT, testUIntVal));
    TEST_ASSERT_GREATER_THAN_UINT32(0, testPreferences.putInt(KEY_INT, testIntVal));
    TEST_ASSERT_GREATER_THAN_UINT32(0, testPreferences.putString(KEY_STR, testStrVal));
    TEST_ASSERT_GREATER_THAN_UINT32(0, testPreferences.putBool(KEY_BOOL, testBoolVal));

    // 3. Close namespace (commits changes)
    testPreferences.end();
    delay(50); // Give time for changes to be written

    // 4. Re-open namespace for reading
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, true)); // Read-only mode

    // 5. Read back values and assert they match
    TEST_ASSERT_EQUAL_UINT32(testUIntVal, testPreferences.getUInt(KEY_UINT, 0)); // Use 0 as default
    TEST_ASSERT_EQUAL_INT(testIntVal, testPreferences.getInt(KEY_INT, 0));       // Use 0 as default
    TEST_ASSERT_EQUAL_STRING(testStrVal.c_str(), testPreferences.getString(KEY_STR, "").c_str()); // Use "" as default
    TEST_ASSERT_EQUAL(testBoolVal, testPreferences.getBool(KEY_BOOL, false));    // Use false as default

    // 6. Close namespace
    testPreferences.end();
}

// Test that get... functions return the correct default value when a key doesn't exist
void test_nvs_get_default_values(void) {
    // Define default values to test against
    unsigned int defaultUInt = 999;
    int defaultInt = -111;
    String defaultStr = "Default";
    bool defaultBool = true; // Test with a non-false default

    // 1. Open namespace (it should be empty due to setUp)
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, true)); // Read-only mode

    // 2. Attempt to get non-existent keys and check against defaults
    TEST_ASSERT_EQUAL_UINT32(defaultUInt, testPreferences.getUInt(KEY_NONEXISTENT, defaultUInt));
    TEST_ASSERT_EQUAL_INT(defaultInt, testPreferences.getInt(KEY_NONEXISTENT, defaultInt));
    TEST_ASSERT_EQUAL_STRING(defaultStr.c_str(), testPreferences.getString(KEY_NONEXISTENT, defaultStr).c_str());
    TEST_ASSERT_EQUAL(defaultBool, testPreferences.getBool(KEY_NONEXISTENT, defaultBool));

    // 3. Close namespace
    testPreferences.end();
}

// Test clearing all keys within the namespace
void test_nvs_clear_namespace(void) {
    // 1. Open and write some dummy data
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, false));
    testPreferences.putInt("tempKey1", 10);
    testPreferences.putString("tempKey2", "abc");
    // Check if a key exists (optional but good practice)
    TEST_ASSERT_TRUE(testPreferences.isKey("tempKey1"));
    testPreferences.end();
    delay(50);

    // 2. Open again and clear
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, false));
    TEST_ASSERT_TRUE(testPreferences.clear()); // Test the clear operation
    // Verify a key no longer exists
    TEST_ASSERT_FALSE(testPreferences.isKey("tempKey1"));
    testPreferences.end();
    delay(50);

    // 3. Open one more time (read-only) and verify get returns default
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, true));
    TEST_ASSERT_EQUAL_INT(0, testPreferences.getInt("tempKey1", 0)); // Should get default 0 now
    testPreferences.end();
}

// Test removing a specific key
void test_nvs_remove_key(void) {
    // 1. Open and write multiple keys
    TEST_ASSERT_TRUE(testPreferences.begin(NVS_TEST_NAMESPACE, false));
    testPreferences.putInt("keyKeep", 100);
    testPreferences.putInt("keyRemove", 200);
    TEST_ASSERT_TRUE(testPreferences.isKey("keyKeep"));
    TEST_ASSERT_TRUE(testPreferences.isKey("keyRemove"));

    // 2. Remove one specific key
    TEST_ASSERT_TRUE(testPreferences.remove("keyRemove"));

    // 3. Verify the removed key is gone, but the other remains
    TEST_ASSERT_TRUE(testPreferences.isKey("keyKeep"));
    TEST_ASSERT_FALSE(testPreferences.isKey("keyRemove"));
    TEST_ASSERT_EQUAL_INT(100, testPreferences.getInt("keyKeep", 0));
    TEST_ASSERT_EQUAL_INT(0, testPreferences.getInt("keyRemove", 0)); // Should get default now

    // 4. Close namespace
    testPreferences.end();
}

// --- Main Setup and Loop ---

void setup() {
    // Start Serial for test output
    Serial.begin(115200);
    // Optional: Wait for serial connection (useful for some boards)
    // while (!Serial);
    delay(2000); // Wait a bit for platform stability

    UNITY_BEGIN(); // IMPORTANT: Start Unity framework

    // Run the test cases
    RUN_TEST(test_nvs_begin);
    RUN_TEST(test_nvs_write_read_types);
    RUN_TEST(test_nvs_get_default_values);
    RUN_TEST(test_nvs_clear_namespace);
    RUN_TEST(test_nvs_remove_key);

    UNITY_END(); // IMPORTANT: Finish Unity framework and report results
}

void loop() {
    // Nothing needed here, tests run only once in setup()
    delay(500);
}