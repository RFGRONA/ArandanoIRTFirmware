/**
 * @file OV2640Sensor.h
 * @brief Defines a wrapper class for the OV2640 camera module using the ESP-IDF driver.
 *
 * Provides methods to initialize the camera hardware based on predefined pin
 * configurations (specific to the project's ESP32-S3 board), capture JPEG images,
 * and properly deinitialize the camera to release resources. Leverages the
 * `esp_camera.h` functions from the ESP-IDF framework.
 */
#ifndef OV2640SENSOR_H
#define OV2640SENSOR_H

#include <Arduino.h>
#include "esp_camera.h"      // ESP-IDF camera driver functions and types (camera_fb_t, etc.)
#include "esp_heap_caps.h"   // For memory allocation functions like ps_malloc (allocating in PSRAM)

/**
 * @class OV2640Sensor
 * @brief Wrapper class simplifying interaction with an OV2640 camera via the ESP-IDF driver.
 *
 * Encapsulates the initialization, image capture (JPEG format), and deinitialization
 * logic for the camera module, using hardware-specific pin configurations defined
 * in the implementation file.
 */
class OV2640Sensor {
public:
    /**
     * @brief Default constructor for the OV2640 sensor wrapper.
     * Does not perform any hardware initialization; call begin() for that.
     */
    OV2640Sensor();

    /**
     * @brief Initializes the OV2640 camera module using the ESP-IDF driver.
     *
     * Configures the camera based on pin definitions and settings (clock frequency,
     * pixel format = JPEG, frame size = VGA, JPEG quality, framebuffer count/location = PSRAM)
     * specified within the implementation (.cpp file). It then calls the underlying
     * `esp_camera_init` function.
     * This method should be called once in the `setup()` function.
     *
     * @return `true` if camera initialization was successful, `false` otherwise. Check Serial
     * output for ESP-IDF driver error codes if initialization fails (requires Serial.begin).
     */
    bool begin();

    /**
     * @brief Captures a single JPEG image frame from the camera.
     *
     * This function requests a framebuffer from the camera driver using `esp_camera_fb_get()`.
     * If successful, it allocates a new buffer in PSRAM (using `ps_malloc`) of the exact
     * size required for the captured JPEG data. It then copies the JPEG data from the
     * driver's framebuffer into this newly allocated buffer. Finally, it returns the
     * driver's framebuffer using `esp_camera_fb_return()` so the driver can reuse it.
     *
     * @param[out] length Reference to a size_t variable. On success, this variable will be
     * filled with the size of the captured JPEG image data in bytes. On failure, it will be set to 0.
     *
     * @return Pointer (uint8_t*) to the newly allocated buffer in PSRAM containing the JPEG data.
     * The caller is responsible for freeing this memory using `free()` (or `ps_free()`)
     * once the image data is no longer needed.
     * Returns `nullptr` if the camera capture failed (e.g., `esp_camera_fb_get` failed)
     * or if memory allocation for the copy buffer failed.
     */
    uint8_t* captureJPEG(size_t &length);

    /**
     * @brief Deinitializes the camera driver and releases associated hardware resources.
     *
     * Calls the underlying `esp_camera_deinit()` function. This is crucial to free
     * resources like DMA channels, I2S peripheral, and allocated framebuffers,
     * especially before entering deep sleep modes or reconfiguring the camera.
     */
    void end();

private:
    // No private members are needed for this basic wrapper.
    // The camera state (configuration, framebuffers) is managed internally
    // by the ESP-IDF esp_camera driver functions.
};

#endif // OV2640SENSOR_H