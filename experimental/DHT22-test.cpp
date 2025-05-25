#include <Arduino.h>            // Include the Arduino library for basic Arduino functionality
#include <DHT.h>                // Include the DHT library to interact with the DHT temperature and humidity sensor

#define DHTPIN 38               // Define the pin where the DHT22 sensor is connected
#define DHTTYPE DHT22           // Define the type of DHT sensor (DHT22 in this case)

DHT dht(DHTPIN, DHTTYPE);      // Create an instance of the DHT sensor, specifying the pin and sensor type

void setup() {
  Serial.begin(115200);        // Initialize serial communication at a baud rate of 115200
  Serial.println("\nStarting DHT22 test");  // Print a startup message to the serial monitor
  dht.begin();                 // Initialize the DHT sensor
}

void loop() {
  delay(2000);                 // Wait for 2 seconds before reading sensor values again

  float h = dht.readHumidity();      // Read the humidity value from the DHT22 sensor
  float t = dht.readTemperature();   // Read the temperature value from the DHT22 sensor

  // Check if the readings are valid. If not, print an error message.
  if (isnan(h) || isnan(t)) {
    Serial.println("Error reading the sensor!");  // Error message if readings are invalid
    return;                                     // Exit the loop and retry
  }

  // Print the humidity and temperature values to the serial monitor
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.print("%\t");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println("°C");
}