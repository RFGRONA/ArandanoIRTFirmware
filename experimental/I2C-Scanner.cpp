// I2C Scanner
#include <Wire.h>
#include <Arduino.h>

void setup() {
  Wire.begin(47, 21); // Inicia I2C con tus pines
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- Escaner de Direcciones I2C ---");
}

void loop() {
  byte error, address;
  int nDevices;
  Serial.println("Escaneando...");
  nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Dispositivo I2C encontrado en la direccion 0x");
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) {
      Serial.print("Error desconocido en la direccion 0x");
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println("No se encontraron dispositivos I2C\n");
  else
    Serial.println("Escaneo finalizado\n");
  delay(5000); // Espera 5 segundos para volver a escanear
}