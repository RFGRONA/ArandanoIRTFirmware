#include "esp_camera.h"
#include <Arduino.h>

// Configuración de pines para ESP32-S3-WROOM-1 N16R8 
#define PWDN_GPIO_NUM  -1     // -1 si no está conectado (Power Down)
#define RESET_GPIO_NUM -1     // -1 si no está conectado, o el pin de reset si lo tienes.
#define XCLK_GPIO_NUM  15     // CAM_XCLK
#define SIOD_GPIO_NUM  4      // CAM_SIOD
#define SIOC_GPIO_NUM  5      // CAM_SIOC

#define Y9_GPIO_NUM   16     // CAM_Y9
#define Y8_GPIO_NUM   17     // CAM_Y8
#define Y7_GPIO_NUM   18     // CAM_Y7
#define Y6_GPIO_NUM   12     // CAM_Y6
#define Y5_GPIO_NUM   10     // CAM_Y5
#define Y4_GPIO_NUM   8      // CAM_Y4
#define Y3_GPIO_NUM   9      // CAM_Y3
#define Y2_GPIO_NUM   11     // CAM_Y2

#define VSYNC_GPIO_NUM 6      // CAM_VSYNC
#define HREF_GPIO_NUM  7      // CAM_HREF
#define PCLK_GPIO_NUM  13     // CAM_PCLK

void printBase64(const uint8_t* data, size_t length);

void setup() {
  Serial.begin(115200);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_QVGA;   // Resolución más baja para pruebas.
  config.pixel_format = PIXFORMAT_JPEG; //  JPEG para esta prueba
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_DRAM; // Usar DRAM ya que no hay PSRAM
  config.jpeg_quality = 10;       // Calidad JPEG (10 es buena calidad, 63 es baja calidad)
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return; // Detiene la ejecución.
  }

  Serial.println("Camera initialized");

  // Puedes ajustar otros parámetros del sensor aquí si es necesario, por ejemplo:
  // sensor_t * s = esp_camera_sensor_get();
  // s->set_brightness(s, 1);  // Ajustar brillo, etc.

}

void loop() {
  camera_fb_t * fb = esp_camera_fb_get();  // Obtiene el framebuffer (imagen JPEG)

  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    return;
  }

  Serial.printf("Captured image: %d bytes\n", fb->len);

  // Imprimir los primeros 64 bytes de la imagen JPEG (en hexadecimal)
  Serial.println("First 64 bytes of JPEG data (hex):");
  for (int i = 0; i < 64; i++) {
    Serial.printf("%02X ", fb->buf[i]);
    if ((i + 1) % 16 == 0) {
      Serial.println(); // Nueva línea cada 16 bytes.
    }
  }
  Serial.println();
    // Imprimir en Base64.
  Serial.println("Imagen en base64:");    
  printBase64(fb->buf, fb->len);
  Serial.println();

  esp_camera_fb_return(fb); // *SIEMPRE* devuelve el framebuffer.

  delay(5000); // Espera 5 segundos antes de la siguiente captura.
}


// Función para imprimir en Base64.
void printBase64(const uint8_t* data, size_t length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = ((length + 2) / 3) * 4; // Calcula la longitud de la salida Base64

    for (size_t i = 0; i < length; i += 3) {
        // Obtiene 3 bytes de entrada
        uint32_t chunk = (data[i] << 16);
        if (i + 1 < length) chunk |= (data[i + 1] << 8);
        if (i + 2 < length) chunk |= data[i + 2];

        // Divide el chunk de 24 bits en cuatro índices de 6 bits
        uint8_t index1 = (chunk >> 18) & 0x3F;
        uint8_t index2 = (chunk >> 12) & 0x3F;
        uint8_t index3 = (i + 1 < length) ? ((chunk >> 6) & 0x3F) : 64; // Rellena si es necesario
        uint8_t index4 = (i + 2 < length) ? (chunk & 0x3F) : 64;        // Rellena si es necesario

        // Imprime los caracteres Base64 correspondientes
        Serial.write(base64_chars[index1]);
        Serial.write(base64_chars[index2]);
        Serial.write((index3 < 64) ? base64_chars[index3] : '='); // '=' para relleno
        Serial.write((index4 < 64) ? base64_chars[index4] : '='); // '=' para relleno
    }

    Serial.println();
}