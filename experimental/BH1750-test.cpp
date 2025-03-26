#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

void setup() {
  Serial.begin(115200);

  Wire.begin(42, 41);

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 inicializado correctamente"));
  } else {
    Serial.println(F("Error al inicializar BH1750"));
    while (1);
  }
}

void loop() {
  float lux = lightMeter.readLightLevel();

  Serial.print("Luz: ");
  Serial.print(lux);
  Serial.println(" lx");

  delay(1000);
}