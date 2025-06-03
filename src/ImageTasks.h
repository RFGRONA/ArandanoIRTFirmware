// src/ImageTasks.h
#ifndef IMAGE_TASKS_H
#define IMAGE_TASKS_H

#include <Arduino.h>
// Include headers for types used in parameters
#include "OV2640Sensor.h"
#include "MLX90640Sensor.h"
#include "API.h"
#include "LEDStatus.h"
#include "ConfigManager.h" 

// --- Function Prototypes for ImageTasks ---
/** @brief Orchestrates capturing and sending image data tasks.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param visCamera Reference to OV2640 camera.
 * @param thermalSensor Reference to MLX90640 thermal sensor.
 * @param sysLed Reference to LEDStatus object.
 * @param[out] jpegImage Pointer to a uint8_t* for the JPEG image buffer.
 * @param[out] jpegLength Reference to size_t for the JPEG length.
 * @param[out] thermalData Pointer to a float* for the thermal data buffer.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return True if images captured AND sent successfully.
 */
bool performImageTasks_Img(Config& cfg, API& api_obj, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                           uint8_t** jpegImage, size_t& jpegLength, float** thermalData, float internalTempForLog, float internalHumForLog);

/** @brief Captures thermal and visual image data into allocated buffers.
 * @param visCamera Reference to OV2640 camera.
 * @param thermalSensor Reference to MLX90640 thermal sensor.
 * @param sysLed Reference to LEDStatus object for status indication.
 * @param[out] jpegImage Pointer to a uint8_t* for the JPEG image buffer.
 * @param[out] jpegLength Reference to size_t for the JPEG length.
 * @param[out] thermalData Pointer to a float* for the thermal data buffer.
 * @param cfg Reference to global Config (for logUrl if logging directly).
 * @param api_obj Reference to the API object (for logUrl and token if logging directly).
 * @param internalTempForLog Internal temperature to include in logs if logging directly.
 * @param internalHumForLog Internal humidity to include in logs if logging directly.
 * @return True if both images captured successfully.
 */
bool captureImages_Img(OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                       uint8_t** jpegImage, size_t& jpegLength, float** thermalData,
                       Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog);

/** @brief Sends captured image data (thermal and visual) to the image API endpoint.
 * @param cfg Reference to global Config.
 * @param api_obj Reference to the API object.
 * @param jpegImage Pointer to the JPEG image buffer.
 * @param jpegLength Length of the JPEG image.
 * @param thermalData Pointer to the thermal data buffer.
 * @param sysLed Reference to LEDStatus object for status indication.
 * @param internalTempForLog Internal temperature to include in logs.
 * @param internalHumForLog Internal humidity to include in logs.
 * @return True if data sent successfully.
 */
bool sendImageData_Img(Config& cfg, API& api_obj, uint8_t* jpegImage, size_t jpegLength, float* thermalData, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog);


/** @brief Captures a thermal data frame and copies it into a newly allocated buffer.
 * @param thermalSensor Reference to MLX90640 thermal sensor.
 * @param[out] thermalDataBuffer Pointer to a float* for the thermal data buffer.
 * @param cfg Reference to global Config (for logUrl if logging directly).
 * @param api_obj Reference to the API object (for logUrl and token if logging directly).
 * @param internalTempForLog Internal temperature to include in logs if logging directly.
 * @param internalHumForLog Internal humidity to include in logs if logging directly.
 * @return True if successful.
 */
bool captureAndCopyThermalData_Img(MLX90640Sensor& thermalSensor, float** thermalDataBuffer, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog);

/** @brief Captures a visual JPEG image into a buffer allocated by the camera driver.
 * @param visCamera Reference to OV2640 camera.
 * @param[out] jpegImageBuffer Pointer to a uint8_t* for the JPEG image buffer.
 * @param[out] jpegLength Reference to size_t for the JPEG length.
 * @param cfg Reference to global Config (for logUrl if logging directly).
 * @param api_obj Reference to the API object (for logUrl and token if logging directly).
 * @param internalTempForLog Internal temperature to include in logs if logging directly.
 * @param internalHumForLog Internal humidity to include in logs if logging directly.
 * @return True if successful.
 */
bool captureVisualJPEG_Img(OV2640Sensor& visCamera, uint8_t** jpegImageBuffer, size_t& jpegLength, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog);

#endif // IMAGE_TASKS_H