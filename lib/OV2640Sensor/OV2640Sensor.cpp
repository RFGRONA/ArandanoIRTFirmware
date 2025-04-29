/**
 * @file OV2640Sensor.cpp
 * @brief Implements the OV2640Sensor wrapper class using the ESP-IDF camera driver.
 */
#include "OV2640Sensor.h"

// --- Hardware Pin Configuration ---
// Pin definition for the specific ESP32-S3 board used (e.g., ESP32-S3-WROOM-1 N16R8 variant)
// Modify these pins according to your specific hardware connections.
#define PWDN_GPIO_NUM     -1 ///< Power Down pin GPIO number (-1 if not used/wired).
#define RESET_GPIO_NUM    -1 ///< Reset pin GPIO number (-1 if not used/wired).
#define XCLK_GPIO_NUM     15 ///< External Clock input pin GPIO number.
#define SIOD_GPIO_NUM     4  ///< SCCB (I2C-like) Data pin (Serial Input/Output Data) GPIO number.
#define SIOC_GPIO_NUM     5  ///< SCCB (I2C-like) Clock pin (Serial Input Clock) GPIO number.

// Camera Data pins (Parallel 8-bit interface)
#define Y9_GPIO_NUM       16 ///< Camera Data Bus line 7 (D7) GPIO number.
#define Y8_GPIO_NUM       17 ///< Camera Data Bus line 6 (D6) GPIO number.
#define Y7_GPIO_NUM       18 ///< Camera Data Bus line 5 (D5) GPIO number.
#define Y6_GPIO_NUM       12 ///< Camera Data Bus line 4 (D4) GPIO number.
#define Y5_GPIO_NUM       10 ///< Camera Data Bus line 3 (D3) GPIO number.
#define Y4_GPIO_NUM       8  ///< Camera Data Bus line 2 (D2) GPIO number.
#define Y3_GPIO_NUM       9  ///< Camera Data Bus line 1 (D1) GPIO number.
#define Y2_GPIO_NUM       11 ///< Camera Data Bus line 0 (D0) GPIO number.

// Camera Synchronization pins
#define VSYNC_GPIO_NUM    6  ///< Vertical Sync signal GPIO number.
#define HREF_GPIO_NUM     7  ///< Horizontal Reference signal GPIO number.
#define PCLK_GPIO_NUM     13 ///< Pixel Clock signal GPIO number.
// --------------------------------

/**
 * @brief Default constructor implementation.
 * Does nothing, as initialization requires hardware context provided in begin().
 */
OV2640Sensor::OV2640Sensor() {
    // Intentionally empty.
}

/**
 * @brief Initializes the camera hardware and driver.
 */
bool OV2640Sensor::begin() {
    // Configuration structure required by the esp_camera_init function.
    camera_config_t config;

    // --- Populate Pin Configuration ---
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

    // --- Populate Clock, Format, Frame, and Buffer Configuration ---
    config.xclk_freq_hz = 20000000;       // Set camera external clock frequency (e.g., 20MHz).
    config.ledc_channel = LEDC_CHANNEL_0; // Use LEDC channel 0 for generating XCLK.
    config.ledc_timer = LEDC_TIMER_0;     // Use LEDC timer 0 for XCLK generation.

    config.pixel_format = PIXFORMAT_JPEG; // Set desired pixel format (JPEG for direct use).
                                          // Other options: PIXFORMAT_RGB565, PIXFORMAT_YUV422 etc.
    config.frame_size = FRAMESIZE_VGA;    // Set frame size (640x480). Others: SVGA(800x600), XGA(1024x768), etc.
    config.jpeg_quality = 10;             // Set JPEG quality (0-63). Lower number = higher quality, larger file size.

    // Framebuffer configuration
    config.fb_count = 1;                  // Number of framebuffers. 1 means capture stops until buffer is returned.
                                          // 2 allows for faster continuous capture but uses more PSRAM.
    config.fb_location = CAMERA_FB_IN_PSRAM; // Store framebuffers in external PSRAM (essential for larger frames).
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Capture strategy: capture when buffer is free.
                                               // CAMERA_GRAB_LATEST drops older frames if processing is slow.

    // --- Initialize the Camera Driver ---
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        // Initialization failed. Log the ESP-IDF error code for debugging.
        // Ensure Serial is initialized (`Serial.begin(...)`) before using Serial.printf.
        // Serial.printf("Camera Init Failed with error 0x%x (%s)\n", err, esp_err_to_name(err));
        return false; // Return failure indication.
    }

    // Camera initialized successfully.
    // Optional: Configure sensor settings like brightness, contrast etc. here if needed,
    // using functions like `sensor_t * s = esp_camera_sensor_get(); s->set_...(s, value);`
    // Serial.println("Camera initialized successfully.");
    return true; // Return success indication.
}

/**
 * @brief Captures a single JPEG image frame into a newly allocated buffer.
 */
uint8_t* OV2640Sensor::captureJPEG(size_t &length) {
    // Reset length output parameter
    length = 0;

    // 1. Acquire a framebuffer containing the captured image from the driver.
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        // Failed to get a framebuffer. Camera might be busy or disconnected.
        // Serial.println("Camera capture failed: esp_camera_fb_get returned NULL.");
        return nullptr; // Return null pointer indicating failure.
    }

    // Check if the format is indeed JPEG (as configured in begin)
    if(fb->format != PIXFORMAT_JPEG){
        // Serial.printf("Camera capture failed: Unexpected format %d, expected PIXFORMAT_JPEG\n", fb->format);
        esp_camera_fb_return(fb); // Return the incorrect buffer
        return nullptr;
    }

    // 2. Allocate memory in PSRAM to store a copy of the JPEG data.
    // Using ps_malloc ensures allocation in external PSRAM if available.
    // The caller of captureJPEG is responsible for freeing this buffer later.
    uint8_t *jpegData = (uint8_t *)ps_malloc(fb->len);
    if (!jpegData) {
        // Failed to allocate memory for the copy buffer. Likely out of PSRAM.
        // Serial.printf("Camera capture failed: Failed to allocate %d bytes in PSRAM for JPEG copy.\n", fb->len);
        // IMPORTANT: Must return the original framebuffer to the driver even if allocation fails.
        esp_camera_fb_return(fb);
        return nullptr; // Return null pointer indicating failure.
    }

    // 3. Copy the JPEG data from the driver's buffer (fb->buf) to our new buffer (jpegData).
    memcpy(jpegData, fb->buf, fb->len);
    length = fb->len; // Set the output parameter to the size of the copied data.

    // 4. Return the driver's framebuffer buffer so it can be reused for future captures.
    // This is critical, otherwise the driver will run out of buffers.
    esp_camera_fb_return(fb);

    // Optional: Log success and image size.
    // Serial.printf("Image captured successfully. Size: %d bytes. Buffer at: 0x%X\n", length, (uint32_t)jpegData);

    // 5. Return the pointer to the *copied* JPEG data buffer in PSRAM.
    return jpegData;
}

/**
 * @brief Deinitializes the camera driver, releasing resources.
 */
void OV2640Sensor::end() {
    // Calls the ESP-IDF function to properly shut down the camera driver
    // and release associated hardware resources (I2S, DMA, GPIOs used by camera).
    esp_camera_deinit();
    // Optional: Log deinitialization.
    // Serial.println("Camera deinitialized.");
}