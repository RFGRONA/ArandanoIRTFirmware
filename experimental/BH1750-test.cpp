#include <Wire.h>                // Include the Wire library for I2C communication
#include <BH1750.h>               // Include the BH1750 library to interact with the light sensor

BH1750 lightMeter;               // Create an instance of the BH1750 light sensor

void setup() {
  Serial.begin(115200);          // Initialize serial communication at a baud rate of 115200

  Wire.begin(47, 21);            // Start I2C communication with specified SDA and SCL pins

  // Initialize the BH1750 light sensor in continuous high-resolution mode
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 initialized successfully"));  // If initialization is successful, print a success message
  } else {
    Serial.println(F("Error initializing BH1750"));        // If initialization fails, print an error message
    while (1);  // Keep the program in an infinite loop in case of error to prevent further execution
  }
}

void loop() {
  float lux = lightMeter.readLightLevel();  // Read the light level (lux) from the sensor

  Serial.print("Light: ");                 // Print the label "Light: "
  Serial.print(lux);                       // Print the light level (lux)
  Serial.println(" lx");                   // Print "lx" as the unit of measurement for light intensity

  delay(1000);                             // Wait for 1 second before reading again
}