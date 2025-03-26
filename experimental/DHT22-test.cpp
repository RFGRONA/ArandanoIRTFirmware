#include <Arduino.h>
#include <DHT.h>

#define DHTPIN 38       
#define DHTTYPE DHT22  

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial.println("\nIniciando prueba del DHT22");
  dht.begin();
}

void loop() {
  delay(2000);

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Error leyendo el sensor!");
    return;
  }

  Serial.print("Humedad: ");
  Serial.print(h);
  Serial.print("%\t");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.println("°C");
}