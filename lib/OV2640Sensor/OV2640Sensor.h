#ifndef OV2640SENSOR_H
#define OV2640SENSOR_H

#include <Arduino.h>
#include "esp_camera.h"     // ESP-IDF camera driver functions
#include "esp_heap_caps.h"  // For memory allocation functions like ps_malloc

/**
 * @file OV2640Sensor.h
 * @brief Wrapper class for the OV2640 camera module using the ESP-IDF driver.
 *
 * Provides methods to initialize the camera hardware based on predefined pin
 * configurations for ESP32-S3, capture JPEG images, and deinitialize the camera.
 */
class OV2640Sensor {
public:
  /**
   * @brief Default constructor for the OV2640 sensor wrapper.
   */
  OV2640Sensor();

  /**
   * @brief Initializes the OV2640 camera module.
   *
   * Configures the camera pins, clock frequency, pixel format (JPEG),
   * frame size (VGA), framebuffer location (PSRAM), and other parameters
   * using the esp_camera_init function.
   * Call this once in your setup.
   * @return True if camera initialization was successful, False otherwise.
   */
  bool begin();

  /**
   * @brief Captures a single JPEG image from the camera.
   *
   * Gets a framebuffer from the camera driver, copies the JPEG data into a
   * newly allocated buffer in PSRAM, and returns the pointer to this buffer.
   * The original framebuffer is returned to the driver.
   *
   * @param[out] length Reference to a size_t variable that will be filled with the captured JPEG image size in bytes.
   * Set to 0 on failure.
   * @return Pointer to the buffer containing the JPEG data. The caller is responsible
   * for freeing this memory using free() or ps_free() when done.
   * Returns nullptr if capture failed or memory allocation failed.
   */
  uint8_t* captureJPEG(size_t &length);

  /**
   * @brief Deinitializes the camera driver and releases associated resources.
   *
   * Calls esp_camera_deinit(). It's crucial to call this before entering
   * deep sleep to free DMA, I2S, and other resources used by the camera.
   */
  void end();

private:
  // No private members needed for this basic wrapper,
  // state is managed by the underlying esp_camera driver.
};

#endif // OV2640SENSOR_H