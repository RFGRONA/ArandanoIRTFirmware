#include "esp_camera.h"   // Include the ESP32 camera library for camera functionalities
#include <Arduino.h>       // Include the core Arduino library for basic functions

// Pin configuration for ESP32-S3-WROOM-1 N16R8 camera module
#define PWDN_GPIO_NUM  -1     // -1 if not connected (Power Down pin)
#define RESET_GPIO_NUM -1     // -1 if not connected, or pin for reset if used
#define XCLK_GPIO_NUM  15     // XCLK pin
#define SIOD_GPIO_NUM  4      // SCCB SDA pin
#define SIOC_GPIO_NUM  5      // SCCB SCL pin

#define Y9_GPIO_NUM   16     // Camera data pins (Y9 - Y2)
#define Y8_GPIO_NUM   17
#define Y7_GPIO_NUM   18
#define Y6_GPIO_NUM   12
#define Y5_GPIO_NUM   10
#define Y4_GPIO_NUM   8
#define Y3_GPIO_NUM   9
#define Y2_GPIO_NUM   11

#define VSYNC_GPIO_NUM 6     // VSYNC pin (vertical sync)
#define HREF_GPIO_NUM  7     // HREF pin (horizontal reference)
#define PCLK_GPIO_NUM  13    // Pixel Clock pin

void printBase64(const uint8_t* data, size_t length); // Function prototype to print image in Base64

void setup() {
  Serial.begin(115200);         // Initialize serial communication at 115200 baud
  Serial.println();

  camera_config_t config;       // Create a camera configuration struct
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  // Pin assignments for camera data lines
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
  config.xclk_freq_hz = 20000000;  // Set the XCLK frequency to 20MHz
  config.frame_size = FRAMESIZE_QVGA;  // Set the frame size to QVGA for low resolution (test mode)
  config.pixel_format = PIXFORMAT_JPEG; // Set JPEG format for image capture
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_DRAM;  // Use DRAM as there is no PSRAM
  config.jpeg_quality = 10;       // JPEG quality (10 gives good quality, 63 is low)
  config.fb_count = 1;            // Use one framebuffer

  // Camera initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return; // Stop execution if initialization fails
  }

  Serial.println("Camera initialized");

  // You can adjust other sensor parameters here if needed, such as:
  // sensor_t * s = esp_camera_sensor_get();
  // s->set_brightness(s, 1);  // Adjust brightness, etc.
}

void loop() {
  camera_fb_t * fb = esp_camera_fb_get();  // Get the framebuffer (JPEG image)

  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);  // Wait before trying again
    return;
  }

  Serial.printf("Captured image: %d bytes\n", fb->len);  // Print the size of the captured image

  // Print the first 64 bytes of the JPEG image in hexadecimal
  Serial.println("First 64 bytes of JPEG data (hex):");
  for (int i = 0; i < 64; i++) {
    Serial.printf("%02X ", fb->buf[i]);  // Print each byte in hexadecimal
    if ((i + 1) % 16 == 0) {
      Serial.println();  // New line every 16 bytes
    }
  }
  Serial.println();

  // Print the image data in Base64
  Serial.println("Image in Base64:");
  printBase64(fb->buf, fb->len);  // Convert and print the image in Base64 encoding
  Serial.println();

  esp_camera_fb_return(fb);  // Always return the framebuffer to free memory

  delay(5000);  // Wait 5 seconds before capturing the next image
}

// Function to print image data in Base64
void printBase64(const uint8_t* data, size_t length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  // Base64 characters set
    size_t output_length = ((length + 2) / 3) * 4; // Calculate the length of the Base64 output

    for (size_t i = 0; i < length; i += 3) {
        // Get 3 bytes of input
        uint32_t chunk = (data[i] << 16);  // First byte shifted 16 bits
        if (i + 1 < length) chunk |= (data[i + 1] << 8);  // Second byte shifted 8 bits
        if (i + 2 < length) chunk |= data[i + 2];  // Third byte

        // Split the 24-bit chunk into four 6-bit values
        uint8_t index1 = (chunk >> 18) & 0x3F;
        uint8_t index2 = (chunk >> 12) & 0x3F;
        uint8_t index3 = (i + 1 < length) ? ((chunk >> 6) & 0x3F) : 64;  // Pad if necessary
        uint8_t index4 = (i + 2 < length) ? (chunk & 0x3F) : 64;  // Pad if necessary

        // Write the corresponding Base64 characters
        Serial.write(base64_chars[index1]);
        Serial.write(base64_chars[index2]);
        Serial.write((index3 < 64) ? base64_chars[index3] : '=');  // Use '=' for padding
        Serial.write((index4 < 64) ? base64_chars[index4] : '=');
    }

    Serial.println();  // End the Base64 output with a new line
}