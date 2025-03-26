#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90640.h>

Adafruit_MLX90640 mlx;
float frame[32 * 24];  

char temperaturaACaracter(float temp) {
  if (temp < 5.0) {
    return ' '; 
  } else if (temp > 40.0) {
    return '#'; 
  } else {
    float escala = (temp - 5.0) / (40.0 - 5.0); 
    if (escala < 0.2) return '.'; 
    else if (escala < 0.4) return ',';
    else if (escala < 0.6) return '-';
    else if (escala < 0.8) return '+';
    else return '*'; 
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(40, 39);  
  Wire.setClock(400000);  

  Serial.println("\n\n=== Test MLX90640 ===");
  Serial.println("Realizando escaneo I2C...");

  byte count = 0;
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Dispositivo encontrado en: 0x");
      Serial.println(i, HEX);
      count++;
      delay(1);
    }
  }
  Serial.print(count);
  Serial.println(" dispositivos encontrados");

  Serial.println("Inicializando MLX90640...");
  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("¡Error de comunicación con el sensor!");
    while (1) delay(10);
  }

  mlx.setMode(MLX90640_CHESS);
  mlx.setResolution(MLX90640_ADC_18BIT);
  mlx.setRefreshRate(MLX90640_4_HZ);

  Serial.println("Configuración exitosa");
  Serial.println("Resolución: 18 bits");
  Serial.println("Modo: Chess");
  Serial.println("Tasa de refresco: 4Hz");
}

void loop() {
  long start = millis();

  if (mlx.getFrame(frame) != 0) {
    Serial.println("Error al leer frame");
    return;
  }

  Serial.println("\nDatos térmicos:");
  for(uint8_t y=0; y<24; y++) {
    for(uint8_t x=0; x<32; x++) {
      Serial.printf("%4.1f ", frame[y*32 + x]);
    }
    Serial.println(); 
  }

  Serial.println("\nMapa de calor simulado:");
  for (uint8_t y = 0; y < 24; y++) {
    for (uint8_t x = 0; x < 32; x++) {
      Serial.print(temperaturaACaracter(frame[y * 32 + x])); 
      Serial.print(" "); 
    }
    Serial.println(); 
  }

  Serial.printf("Tiempo de lectura: %d ms\n", millis() - start);
  delay(5000);
}