// src/ImageTasks.cpp
#include "ImageTasks.h"
#include "ErrorLogger.h"         // For logging errors
#include "MultipartDataSender.h" // For MultipartDataSender::IOThermalAndImageData

#define RGB_CAPTURE_MIN_LIGHT_LEVEL_LUX 1000.0f

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
bool captureAndCopyThermalData_Img(SDManager& sdMgr, TimeManager& timeMgr, MLX90640Sensor& thermalSensor, float** thermalDataBuffer, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) {
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
        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
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
bool captureVisualJPEG_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, uint8_t** jpegImageBuffer, size_t& jpegLength, Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) { 
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
        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
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
bool captureImages_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                       uint8_t** jpegImage, size_t& jpegLength, float** thermalData,
                       Config& cfg, API& api_obj, float internalTempForLog, float internalHumForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] --- Capturing Thermal and Visual Images ---");
    #endif
    *jpegImage = nullptr;
    *thermalData = nullptr;
    jpegLength = 0;

    sysLed.setState(TAKING_DATA);

    if (!captureAndCopyThermalData_Img(sdMgr, timeMgr, thermalSensor, thermalData, cfg, api_obj, internalTempForLog, internalHumForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture or copy thermal data.");
        #endif
        sysLed.setState(ERROR_DATA);

        return false;
    }

    if (!captureVisualJPEG_Img(sdMgr, timeMgr, visCamera, jpegImage, jpegLength, cfg, api_obj, internalTempForLog, internalHumForLog)) {
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
bool sendImageData_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, uint8_t* jpegImage, size_t jpegLength, float* thermalData, LEDStatus& sysLed, float internalTempForLog, float internalHumForLog) {
    // A visual image is now optional, but thermal data is mandatory for this call.
    if (!thermalData) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Error: Invalid data provided to sendImageData_Img (thermalData is null)."));
        #endif
        sysLed.setState(ERROR_DATA);
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, "sendImageData_Img called with null thermal data.", internalTempForLog, internalHumForLog);
        return false;
    }
    
    sysLed.setState(SENDING_DATA);

    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiCaptureDataPath;
    String token = api_obj.getAccessToken();
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Preparing to send capture data via HTTP POST (multipart)..."));
    #endif

    // MultipartDataSender now handles the case where jpegImage is null.
    int httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);

    if (httpCode >= 200 && httpCode < 300) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Capture data sent successfully."));
        #endif

        return true;
    } 
    
    // Handle authentication failure (401)
    if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Capture data send failed (401). Attempting token refresh..."));
        #endif
        int refreshHttpCode = api_obj.performTokenRefresh(); 
        if (refreshHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[ImgTasks] Token refresh successful. Re-trying capture data send..."));
            #endif
            token = api_obj.getAccessToken();
            httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, thermalData, jpegImage, jpegLength);
            if (httpCode >= 200 && httpCode < 300) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[ImgTasks] Capture data sent successfully on retry."));
                #endif

                return true;
            }
        }
    }

    // All other errors
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[ImgTasks] Error sending capture data. Final HTTP Code: %d\n", httpCode);
    #endif
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
 * @param lightLevel Ambient light level, to decide if image capture is needed.
 * @param[out] jpegImage Output for JPEG image buffer.
 * @param[out] jpegLength Output for JPEG length.
 * @param[out] thermalData Output for thermal data.
 * @param internalTempForLog Internal device temperature for logging.
 * @param internalHumForLog Internal device humidity for logging.
 * @return True if image capture AND send were successful.
 */
bool performImageTasks_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                           float lightLevel,
                           uint8_t** jpegImage, size_t& jpegLength, float** thermalData, float internalTempForLog, float internalHumForLog) { 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n[ImgTasks] --- Performing Image Data Tasks (Capture, Send, Archive) ---"));
    #endif

    *jpegImage = nullptr;
    jpegLength = 0;
    *thermalData = nullptr;
    
    sysLed.setState(TAKING_DATA);

    bool captureVisual = (lightLevel >= RGB_CAPTURE_MIN_LIGHT_LEVEL_LUX);
    if (!captureVisual) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] Low light condition (%.2f lux). Skipping visual image capture.\n", lightLevel);
        #endif
    }

    if (!captureAndCopyThermalData_Img(sdMgr, timeMgr, thermalSensor, thermalData, cfg, api_obj, internalTempForLog, internalHumForLog)) {
        return false;
    }

    if (captureVisual) {
        if (!captureVisualJPEG_Img(sdMgr, timeMgr, visCamera, jpegImage, jpegLength, cfg, api_obj, internalTempForLog, internalHumForLog)) {
            if (*thermalData != nullptr) { free(*thermalData); *thermalData = nullptr; }
            return false;
        }
    }

    bool sentSuccessfully = sendImageData_Img(sdMgr, timeMgr, cfg, api_obj, *jpegImage, jpegLength, *thermalData, sysLed, internalTempForLog, internalHumForLog);

    if (sdMgr.isSDAvailable()) {
        String baseFilename = timeMgr.getCurrentTimestampString(true);
        String targetDir = sentSuccessfully ? String(ARCHIVE_CAPTURES_DIR) : String(CAPTURE_PENDING_DIR);
        
        if (*thermalData) {
            String thermalJsonString = MultipartDataSender::createThermalJson(*thermalData);
            if (!thermalJsonString.isEmpty()) {
                sdMgr.writeTextFile(targetDir + "/" + baseFilename + "_thermal.json", thermalJsonString);
            }
        }
        
        if (*jpegImage && jpegLength > 0) {
            sdMgr.writeBinaryFile(targetDir + "/" + baseFilename + "_visual.jpg", *jpegImage, jpegLength);
        }
    }
    
    if (!sentSuccessfully) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Result: Image Task FAILED at Send stage (data saved to pending)."));
        #endif
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Result: Image Task SUCCEEDED (Capture & Send, data archived)."));
    #endif
    return true;
}