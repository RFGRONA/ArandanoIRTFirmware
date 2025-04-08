#include "OV2640Sensor.h"

// Pin definition for the ESP32-S3-WROOM-1 N16R8 Camera Interface
// These pins are specific to the hardware connection used in this project.
#define PWDN_GPIO_NUM   -1 // Power Down pin not used
#define RESET_GPIO_NUM  -1 // Reset pin not used
#define XCLK_GPIO_NUM   15 // External Clock input pin
#define SIOD_GPIO_NUM   4  // SCCB (I2C-like) Data pin (Serial Input/Output Data)
#define SIOC_GPIO_NUM   5  // SCCB (I2C-like) Clock pin (Serial Input Clock)

// Camera Data pins (Parallel interface)
#define Y9_GPIO_NUM     16 // D7
#define Y8_GPIO_NUM     17 // D6
#define Y7_GPIO_NUM     18 // D5
#define Y6_GPIO_NUM     12 // D4
#define Y5_GPIO_NUM     10 // D3
#define Y4_GPIO_NUM     8  // D2
#define Y3_GPIO_NUM     9  // D1
#define Y2_GPIO_NUM     11 // D0

// Camera Sync pins
#define VSYNC_GPIO_NUM  6  // Vertical Sync
#define HREF_GPIO_NUM   7  // Horizontal Reference
#define PCLK_GPIO_NUM   13 // Pixel Clock

// Default constructor - does nothing as initialization is handled in begin()
OV2640Sensor::OV2640Sensor() {}

// Initializes the camera using the ESP-IDF esp_camera driver
bool OV2640Sensor::begin() {
  // Configuration structure for the camera driver
  camera_config_t config;

  // --- Pin Configuration ---
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;

  // --- Clock, Format, Frame, and Buffer Configuration ---
  config.xclk_freq_hz = 20000000;     // External clock frequency (20 MHz)
  config.ledc_channel = LEDC_CHANNEL_0; // LEDC channel for XCLK generation
  config.ledc_timer = LEDC_TIMER_0;     // LEDC timer for XCLK generation
  config.pixel_format = PIXFORMAT_JPEG; // Output format: JPEG
  config.frame_size = FRAMESIZE_VGA;    // Frame size: 640x480 (adjust as needed)
  config.jpeg_quality = 10;             // JPEG quality (0-63, lower is higher quality)
  config.fb_count = 1;                  // Number of framebuffers (1 for single shot mode)
                                        // Using 2 can enable continuous mode if needed, uses more RAM.
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // When to capture the frame into the buffer
  config.fb_location = CAMERA_FB_IN_PSRAM; // Allocate framebuffers in external PSRAM

  // Attempt to initialize the camera driver with the configuration
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    // Log error if initialization fails (using Serial requires Serial.begin first)
    // Serial.printf("Camera Init Failed with error 0x%x\n", err);
    return false; // Return failure
  }

  // Camera initialized successfully
  // Serial.println("Camera initialized"); // Optional debug output
  return true; // Return success
}

// Captures a JPEG image frame from the camera
uint8_t* OV2640Sensor::captureJPEG(size_t &length) {
  // Acquire a framebuffer from the driver
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    // Serial.println("Camera capture failed (fb is null)"); // Optional debug output
    length = 0;       // Set length to 0 on failure
    return nullptr;   // Return null pointer on failure
  }

  // Allocate memory in PSRAM to copy the JPEG data.
  // The caller will be responsible for freeing this memory.
  uint8_t *jpegData = (uint8_t *)ps_malloc(fb->len);
  if (!jpegData) {
    // Serial.println("Failed to allocate memory for JPEG copy"); // Optional debug output
    length = 0;               // Set length to 0 on failure
    esp_camera_fb_return(fb); // IMPORTANT: Return the original framebuffer to the driver
    return nullptr;           // Return null pointer on failure
  }

  // Copy the image data from the driver's framebuffer to the new buffer
  memcpy(jpegData, fb->buf, fb->len);
  length = fb->len; // Set the output length parameter

  // Return the driver's framebuffer so it can be reused
  esp_camera_fb_return(fb);

  // Serial.printf("Image captured: %d bytes\n", length); // Optional debug output

  // Return the pointer to the copied JPEG data
  return jpegData;
}

// Deinitializes the camera driver
void OV2640Sensor::end() {
  // Calls the ESP-IDF function to release camera resources (DMA, I2S, etc.)
  esp_camera_deinit();
  // Serial.println("Camera deinitialized"); // Optional debug output
}