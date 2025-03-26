#include "OV2640Sensor.h"

// Definición de pines para el ESP32-S3-WROOM-1 N16R8
#define PWDN_GPIO_NUM   -1
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM   15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5

#define Y9_GPIO_NUM     16
#define Y8_GPIO_NUM     17
#define Y7_GPIO_NUM     18
#define Y6_GPIO_NUM     12
#define Y5_GPIO_NUM     10
#define Y4_GPIO_NUM     8
#define Y3_GPIO_NUM     9
#define Y2_GPIO_NUM     11

#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM   13

OV2640Sensor::OV2640Sensor() {
  // Constructor (actualmente vacío)
}

bool OV2640Sensor::begin() {
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
  config.frame_size = FRAMESIZE_QVGA;     // Resolución baja para pruebas.
  config.pixel_format = PIXFORMAT_JPEG;   // JPEG para la imagen.
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_DRAM;  // Usar DRAM si no se cuenta con PSRAM.
  config.jpeg_quality = 10;               // Calidad JPEG (10 = buena calidad).
  config.fb_count = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al inicializar la cámara: 0x%x\n", err);
    return false;
  }
  Serial.println("Cámara inicializada");
  return true;
}

uint8_t* OV2640Sensor::captureJPEG(size_t &length) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Error al capturar imagen");
    length = 0;
    return nullptr;
  }

  // Se crea una copia del buffer para que el caller tenga acceso a los datos
  uint8_t *jpegData = (uint8_t *)malloc(fb->len);
  if (!jpegData) {
    Serial.println("Error al asignar memoria para la imagen");
    length = 0;
    esp_camera_fb_return(fb);
    return nullptr;
  }
  
  memcpy(jpegData, fb->buf, fb->len);
  length = fb->len;
  
  esp_camera_fb_return(fb);
  Serial.printf("Imagen capturada: %d bytes\n", length);
  
  return jpegData;
}

void OV2640Sensor::end() {
  esp_camera_deinit();
  Serial.println("Cámara desinicializada");
}