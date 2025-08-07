#include <Arduino.h>              // Include the core Arduino library for basic functions
#include <Wire.h>                 // Include the Wire library for I2C communication
#include <Adafruit_MLX90640.h>    // Include the library for controlling the MLX90640 thermal sensor

Adafruit_MLX90640 mlx;           // Create an instance of the MLX90640 sensor
float frame[32 * 24];            // Array to hold the thermal data frame (32x24 pixels)

char temperatureToChar(float temp) {
  // Convert temperature to a character based on specific ranges
  if (temp < 5.0) {
    return ' ';  // If temperature is below 5.0°C, return space character
  } else if (temp > 40.0) {
    return '#';  // If temperature is above 40.0°C, return hash character
  } else {
    float scale = (temp - 5.0) / (40.0 - 5.0);  // Scale the temperature to fit between 0.0 and 1.0
    if (scale < 0.2) return '.';  // Light temperature
    else if (scale < 0.4) return ',';  // Slightly higher temperature
    else if (scale < 0.6) return '-';  // Moderate temperature
    else if (scale < 0.8) return '+';  // High temperature
    else return '*';  // Very high temperature
  }
}

void setup() {
  Serial.begin(115200);            // Initialize serial communication at 115200 baud rate
  Wire.begin(40, 39);              // Initialize I2C communication with custom SDA (40) and SCL (39) pins
  Wire.setClock(400000);           // Set I2C clock speed to 400kHz for faster communication

  Serial.println("\n\n=== MLX90640 Test ===");
  Serial.println("Scanning I2C devices...");

  byte count = 0;  // Count of devices found on the I2C bus
  for (byte i = 8; i < 120; i++) {  // Scan I2C addresses from 8 to 119
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {  // If no error, device found
      Serial.print("Device found at: 0x");
      Serial.println(i, HEX);  // Print the address of the found device
      count++;
      delay(1);
    }
  }
  Serial.print(count);
  Serial.println(" devices found");

  Serial.println("Initializing MLX90640...");
  // Initialize the MLX90640 sensor with default I2C address
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("Communication error with sensor!");
    while (1) delay(10);  // If there is an error, enter an infinite loop
  }

  // Set the MLX90640 sensor mode and configuration
  mlx.setMode(MLX90640_CHESS);         // Set the sensor to Chess mode
  mlx.setResolution(MLX90640_ADC_18BIT);  // Set the resolution to 18-bit
  mlx.setRefreshRate(MLX90640_4_HZ);    // Set the refresh rate to 4Hz

  // Display configuration success
  Serial.println("Configuration successful");
  Serial.println("Resolution: 18 bits");
  Serial.println("Mode: Chess");
  Serial.println("Refresh rate: 4Hz");
}

void loop() {
  long start = millis();             // Record the start time for reading the frame

  // Read the thermal frame from the sensor
  if (mlx.getFrame(frame) != 0) {
    Serial.println("Error reading frame");
    return;  // If there is an error, exit the loop
  }

  Serial.println("\nThermal Data:");
  // Print the thermal data frame (32x24 pixels)
  for (uint8_t y = 0; y < 24; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      Serial.printf("%4.1f ", frame[y * 32 + x]);  // Print the temperature for each pixel
    }
    Serial.println();  // Move to the next row
  }

  Serial.println("\nSimulated Heat Map:");
  // Print a simulated heat map using characters
  for (uint8_t y = 0; y < 24; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      Serial.print(temperatureToChar(frame[y * 32 + x]));  // Convert temperature to character and print
      Serial.print(" ");
    }
    Serial.println();  // Move to the next row
  }

  // Print the time taken to read the frame
  Serial.printf("Reading time: %d ms\n", millis() - start);
  delay(5000);  // Wait for 5 seconds before the next reading
}
