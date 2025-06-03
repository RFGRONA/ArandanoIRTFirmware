// src/ImageTasks.cpp
#include "ImageTasks.h"
#include "ErrorLogger.h"         // For logging errors
#include "MultipartDataSender.h" // For MultipartDataSender::IOThermalAndImageData

#if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT // For ps_malloc
#include "esp_heap_caps.h"
#endif


/**
 * @brief Captures a thermal data frame and copies it. Logs critical allocation failures.
 * @param thermalSensor Reference to the MLX90640Sensor object.
 * @param[out] thermalDataBuffer Output for the thermal data buffer.
 * @param cfg Reference to global Config for logging.
 * @param api_obj Reference to API object for logging.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if successful.
 */
bool captureAndCopyThermalData_Img(MLX90640Sensor& thermalSensor, float** thermalDataBuffer, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Reading thermal camera frame (MLX90640)...");
    #endif
    *thermalDataBuffer = nullptr;

    if (!thermalSensor.readFrame()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to read thermal frame from MLX90640 sensor.");
        #endif
        return false;
    }

    float* rawThermalData = thermalSensor.getThermalData();
    if (rawThermalData == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to get thermal data pointer from sensor (null).");
        #endif
        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Thermal frame read OK. Allocating buffer for copy...");
    #endif

    const size_t thermalDataSizeInBytes = 32 * 24 * sizeof(float);

    #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT
        *thermalDataBuffer = (float*)ps_malloc(thermalDataSizeInBytes);
    #else
        *thermalDataBuffer = (float*)malloc(thermalDataSizeInBytes);
    #endif

    if (*thermalDataBuffer == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] CRITICAL ERROR: Failed to allocate %zu bytes for thermal data copy!\n", thermalDataSizeInBytes);
        #endif
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Critical failure: Thermal data buffer allocation failed.", 
                             internalTempForLog, internalHumForLog); 
        return false;
    }

    memcpy(*thermalDataBuffer, rawThermalData, thermalDataSizeInBytes);
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Thermal data buffer allocated and frame data copied successfully.");
    #endif
    return true;
}

/**
 * @brief Captures a visual JPEG image. Logs critical allocation failures.
 * @param visCamera Reference to the OV2640Sensor object.
 * @param[out] jpegImageBuffer Output for the JPEG image buffer.
 * @param[out] jpegLength Output for the JPEG length.
 * @param cfg Reference to global Config for logging.
 * @param api_obj Reference to API object for logging.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if successful.
 */
bool captureVisualJPEG_Img(OV2640Sensor& visCamera, uint8_t** jpegImageBuffer, size_t& jpegLength, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Capturing visual JPEG image (OV2640)...");
    #endif
    *jpegImageBuffer = nullptr;
    jpegLength = 0;

    *jpegImageBuffer = visCamera.captureJPEG(jpegLength); // This function allocates memory

    if (*jpegImageBuffer == nullptr || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture JPEG image or allocation failed.");
        #endif
        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Critical failure: JPEG image capture or buffer allocation failed.", 
                             internalTempForLog, internalHumForLog); 
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[ImgTasks] JPEG Image captured successfully. Size: %zu bytes.\n", jpegLength);
    #endif
    return true;
}

/**
 * @brief Orchestrates capturing both thermal and visual images.
 * @param visCamera Reference to the OV2640Sensor object.
 * @param thermalSensor Reference to the MLX90640Sensor object.
 * @param sysLed Reference to the LEDStatus object for visual feedback.
 * @param[out] jpegImage Output for JPEG data.
 * @param[out] jpegLength Output for JPEG length.
 * @param[out] thermalData Output for thermal data.
 * @param cfg Reference to global Config for logging.
 * @param api_obj Reference to API object for logging.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if both captures succeed.
 */
bool captureImages_Img(OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                       uint8_t** jpegImage, size_t& jpegLength, float** thermalData,
                       Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] --- Capturing Thermal and Visual Images ---");
    #endif
    *jpegImage = nullptr;
    *thermalData = nullptr;
    jpegLength = 0;

    LEDState originalLedState = sysLed.getCurrentState();
    sysLed.setState(TAKING_DATA);

    if (!captureAndCopyThermalData_Img(thermalSensor, thermalData, cfg, api_obj, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture or copy thermal data.");
        #endif
        sysLed.setState(ERROR_DATA);

        return false;
    }

    if (!captureVisualJPEG_Img(visCamera, jpegImage, jpegLength, cfg, api_obj, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture visual JPEG image.");
        #endif
        sysLed.setState(ERROR_DATA);

        if (*thermalData != nullptr) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[ImgTasks] Cleaning up thermal data buffer due to JPEG capture failure.");
            #endif
            free(*thermalData);
            *thermalData = nullptr;
        }

        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Thermal and visual images captured successfully.");
    #endif
    if (sysLed.getCurrentState() == TAKING_DATA) {
         sysLed.setState(originalLedState == TAKING_DATA ? ALL_OK : originalLedState);
    }
    return true;
}

/**
 * @brief Sends captured image data to the server.
 * @param cfg Reference to config.
 * @param api_obj Reference to API object.
 * @param jpegImage JPEG image buffer.
 * @param jpegLength JPEG image length.
 * @param thermalData Thermal data buffer.
 * @param sysLed Reference to LEDStatus.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if data sent successfully.
 */
bool sendImageData_Img(Config& cfg, API& api_obj, uint8_t* jpegImage, size_t jpegLength, float* thermalData, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog) {
    if (!jpegImage || jpegLength == 0 || !thermalData) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Error: Invalid data provided to sendImageData_Img."));
        #endif
        sysLed.setState(ERROR_DATA);

        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR,
                             "sendImageData_Img called with invalid parameters (null image/thermal or zero length).",
                             internalTempForLog, internalHumForLog);
        return false;
    }
    LEDState originalLedState = sysLed.getCurrentState();
    sysLed.setState(SENDING_DATA);

    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiCaptureDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Preparing to send thermal and image data via HTTP POST (multipart)..."));
        Serial.println("  Target URL: " + fullUrl);
    #endif

    int httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);

    if (httpCode == 200 || httpCode == 204) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Image and thermal data sent successfully."));
        #endif
        if (sysLed.getCurrentState() == SENDING_DATA) {
            sysLed.setState(originalLedState == SENDING_DATA ? ALL_OK : originalLedState);
        }
        return true;
    } else if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Capture data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_WARNING, 
                             "Capture data send returned 401. Attempting token refresh.", 
                             internalTempForLog, internalHumForLog); 

        int refreshHttpCode = api_obj.performTokenRefresh(); 
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[ImgTasks] Token refresh successful. Re-trying capture data send..."));
            #endif
            ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, 
                                 "Token refreshed successfully after capture data 401.", 
                                 internalTempForLog, internalHumForLog); 
            token = api_obj.getAccessToken();
            httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);
            if (httpCode == 200 || httpCode == 204) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[ImgTasks] Capture data sent successfully on retry."));
                #endif
                 if (sysLed.getCurrentState() == SENDING_DATA || sysLed.getCurrentState() == ERROR_SEND ) {
                    sysLed.setState(originalLedState == SENDING_DATA ? ALL_OK : originalLedState);
                }
                return true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[ImgTasks] Capture data send failed on retry. HTTP Code: %d\n", httpCode);
                #endif
                ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, 
                                     "Capture data send failed on retry after refresh. HTTP: " + String(httpCode), 
                                     internalTempForLog, internalHumForLog); 
            }
        } else { 
             #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[ImgTasks] Token refresh failed after 401. HTTP Code: %d (API logs this failure)\n", refreshHttpCode);
            #endif
        }
    } else { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] Error sending image/thermal data. HTTP Code: %d\n", httpCode);
        #endif
        ErrorLogger::sendLog(logUrl, token, LOG_TYPE_ERROR, 
                             "Failed to send capture data. HTTP Code: " + String(httpCode), 
                             internalTempForLog, internalHumForLog); 
    }

    sysLed.setState(ERROR_SEND);
    return false;
}

/**
 * @brief Orchestrates capturing and sending image data.
 * @param cfg Reference to config.
 * @param api_obj Reference to API object.
 * @param visCamera Reference to visual camera.
 * @param thermalSensor Reference to thermal camera.
 * @param sysLed Reference to LEDStatus.
 * @param[out] jpegImage Output for JPEG image buffer.
 * @param[out] jpegLength Output for JPEG length.
 * @param[out] thermalData Output for thermal data.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if image capture AND send were successful.
 */
bool performImageTasks_Img(Config& cfg, API& api_obj, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                           uint8_t** jpegImage, size_t& jpegLength, float** thermalData, float internalTempForLog, float internalHumForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n[ImgTasks] --- Performing Image Data Tasks (Capture & Send) ---"));
    #endif

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Task Step: Capture Images (Thermal & Visual)"));
    #endif
    // Pass all necessary logging params to captureImages_Img
    if (!captureImages_Img(visCamera, thermalSensor, sysLed, jpegImage, jpegLength, thermalData, cfg, api_obj, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Result: Image Task FAILED at Capture stage."));
        #endif

        String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;
        ErrorLogger::sendLog(logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, "Image capture stage failed.", internalTempForLog, internalHumForLog);
        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Task Step: Images captured successfully."));
    #endif

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Task Step: Send Image Data"));
    #endif

    if (!sendImageData_Img(cfg, api_obj, *jpegImage, jpegLength, *thermalData, sysLed, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Result: Image Task FAILED at Send stage."));
        #endif
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Result: Image Task SUCCEEDED (Capture & Send)."));
    #endif
    return true;
}