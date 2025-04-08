#include <Arduino.h>
#include <Preferences.h> // Include the Preferences library

Preferences preferences; // Create an instance

// Define a namespace and keys for testing
const char* testNamespace = "nvs_test_space";
const char* keyBootCount = "boot_count";
const char* keyTestString = "test_string";
const char* keyTestInt = "test_int";

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial connection
  Serial.println("\n--- NVS (Preferences) Test Sketch ---");

  // --- Reading Phase ---
  Serial.println("Attempting to read previous values from NVS...");
  // Open the namespace in read/write mode (false)
  // If the namespace doesn't exist, it will be created.
  bool initOK = preferences.begin(testNamespace, false);

  if (initOK) {
    // Read the boot counter (unsigned integer)
    // Get value for keyBootCount, if key doesn't exist, return default value 0
    unsigned int bootCount = preferences.getUInt(keyBootCount, 0);
    Serial.printf("  - Previous Boot Count: %u\n", bootCount);

    // Read a test string
    // Get value for keyTestString, if key doesn't exist, return default "Not Set"
    String testString = preferences.getString(keyTestString, "Not Set");
    Serial.printf("  - Previous Test String: '%s'\n", testString.c_str());

    // Read a test integer
    // Get value for keyTestInt, if key doesn't exist, return default value -1
    int testInt = preferences.getInt(keyTestInt, -1);
    Serial.printf("  - Previous Test Int: %d\n", testInt);

    // --- Writing/Updating Phase ---
    Serial.println("Writing/Updating values in NVS...");

    // Increment boot counter and save it back
    bootCount++;
    preferences.putUInt(keyBootCount, bootCount);
    Serial.printf("  - Saved new Boot Count: %u\n", bootCount);

    // Save/overwrite the test string and integer
    String newString = "Hello NVS! Count: " + String(bootCount);
    preferences.putString(keyTestString, newString);
    preferences.putInt(keyTestInt, bootCount * 10); // Example integer value
    Serial.printf("  - Saved new Test String: '%s'\n", newString.c_str());
    Serial.printf("  - Saved new Test Int: %d\n", bootCount * 10);

    // Close the namespace to commit changes to flash
    preferences.end();
    Serial.println("Preferences saved and namespace closed.");

  } else {
    Serial.println("Error: Failed to initialize Preferences namespace!");
    // Consider error handling here - maybe blink an LED
  }

   Serial.println("Setup complete. Reset the device to see the boot count increment.");
   Serial.println("-------------------------------------");
}

void loop() {
  // Nothing needed here for this test, it runs entirely in setup()
  delay(10000); // Delay to avoid spamming reset messages if something goes wrong
}