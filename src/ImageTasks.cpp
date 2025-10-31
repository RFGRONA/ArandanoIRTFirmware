/**
 * @file ImageTasks.cpp
 * @brief Implementa las funciones de orquestación para las tareas de imagen.
 */
#include "ImageTasks.h"
#include "ErrorLogger.h"         // Para registro de errores
#include "MultipartDataSender.h" // Para enviar los datos multipart

///< Lúmenes mínimos para capturar una imagen visual (RGB).
#define RGB_CAPTURE_MIN_LIGHT_LEVEL_LUX 1000.0f

#if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT // Para ps_malloc
#include "esp_heap_caps.h"
#endif


/**
 * @brief Captura un frame térmico y lo *copia* en un nuevo buffer alocado (con `ps_malloc`).
 * Registra errores críticos si la alocación de memoria falla.
 * @note El llamador es responsable de liberar `*thermalDataBuffer` con `free()`.
 */
bool captureAndCopyThermalData_Img(SDManager& sdMgr, TimeManager& timeMgr, MLX90640Sensor& thermalSensor, float** thermalDataBuffer, Config& cfg, API& api_obj, float internalTempForLog) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Reading thermal camera frame (MLX90640)...");
    #endif
    *thermalDataBuffer = nullptr;

    // 1. Leer el frame (la librería interna maneja reintentos/promediado)
    if (!thermalSensor.readFrame()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to read thermal frame from MLX90640 sensor.");
        #endif
        ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, "Failed to read thermal frame from MLX90640.", internalTempForLog);
        return false;
    }

    // 2. Obtener el puntero al buffer interno del sensor
    float* rawThermalData = thermalSensor.getThermalData();
    if (rawThermalData == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to get thermal data pointer from sensor (null).");
        #endif
        ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, "Failed to get thermal data pointer from MLX90640 (null).", internalTempForLog);
        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Thermal frame read OK. Allocating buffer for copy...");
    #endif

    const size_t thermalDataSizeInBytes = 32 * 24 * sizeof(float); // 768 * 4 = 3072 bytes

    // 3. Alojar un *nuevo* buffer (preferiblemente en PSRAM)
    #if CONFIG_SPIRAM_SUPPORT || CONFIG_ESP32_SPIRAM_SUPPORT
        *thermalDataBuffer = (float*)ps_malloc(thermalDataSizeInBytes);
    #else
        *thermalDataBuffer = (float*)malloc(thermalDataSizeInBytes);
    #endif

    // 4. Verificar alocación
    if (*thermalDataBuffer == nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] CRITICAL ERROR: Failed to allocate %zu bytes for thermal data copy!\n", thermalDataSizeInBytes);
        #endif
        // Este es un error crítico, se loguea remotamente si es posible
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Critical failure: Thermal data buffer allocation failed.", 
                             internalTempForLog); 
        return false;
    }

    // 5. Copiar los datos del buffer del sensor al nuevo buffer
    memcpy(*thermalDataBuffer, rawThermalData, thermalDataSizeInBytes);
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Thermal data buffer allocated and frame data copied successfully.");
    #endif
    return true;
}

/**
 * @brief Captura una imagen JPEG visual. La memoria es alocada por `visCamera.captureJPEG`.
 * Registra errores críticos si la captura o alocación fallan.
 * @note El llamador es responsable de liberar `*jpegImageBuffer` con `free()`.
 */
bool captureVisualJPEG_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, uint8_t** jpegImageBuffer, size_t& jpegLength, Config& cfg, API& api_obj, float internalTempForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] Capturing visual JPEG image (OV2640)...");
    #endif
    *jpegImageBuffer = nullptr;
    jpegLength = 0;

    // 1. Llamar a la captura (esta función aloca la memoria internamente)
    *jpegImageBuffer = visCamera.captureJPEG(jpegLength); 

    // 2. Verificar éxito
    if (*jpegImageBuffer == nullptr || jpegLength == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture JPEG image or allocation failed.");
        #endif
        // Error crítico, loguear remotamente
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, 
                             "Critical failure: JPEG image capture or buffer allocation failed.", 
                             internalTempForLog); 
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[ImgTasks] JPEG Image captured successfully. Size: %zu bytes.\n", jpegLength);
    #endif
    return true;
}

/**
 * @brief Orquesta la captura de ambas imágenes (térmica y visual).
 * @note Importante: Si la captura visual falla después de que la térmica
 * tuvo éxito, este método libera el buffer térmico para evitar fugas de memoria.
 */
bool captureImages_Img(SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed,
                       uint8_t** jpegImage, size_t& jpegLength, float** thermalData,
                       Config& cfg, API& api_obj, float internalTempForLog) { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ImgTasks] --- Capturing Thermal and Visual Images ---");
    #endif
    *jpegImage = nullptr;
    *thermalData = nullptr;
    jpegLength = 0;

    sysLed.setState(TAKING_DATA);

    // 1. Capturar datos térmicos (siempre)
    if (!captureAndCopyThermalData_Img(sdMgr, timeMgr, thermalSensor, thermalData, cfg, api_obj, internalTempForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture or copy thermal data.");
        #endif
        sysLed.setState(ERROR_DATA);
        return false;
    }

    // 2. Capturar datos visuales (siempre)
    if (!captureVisualJPEG_Img(sdMgr, timeMgr, visCamera, jpegImage, jpegLength, cfg, api_obj, internalTempForLog)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ImgTasks] Error: Failed to capture visual JPEG image.");
        #endif
        sysLed.setState(ERROR_DATA);

        // --- Limpieza de Memoria ---
        // Si la captura térmica tuvo éxito (*thermalData != nullptr) pero
        // la visual falló, debemos liberar la memoria térmica alocada.
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
 * @brief Envía los datos de captura (multipart/form-data) al servidor.
 * Maneja el reintento por error 401 (token).
 * @note `thermalData` es mandatorio. `jpegImage` es opcional.
 */
bool sendImageData_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, const String& timestamp, uint8_t* jpegImage, size_t jpegLength, float* thermalData, LEDStatus& sysLed, float internalTempForLog) {
    // Los datos térmicos son obligatorios para este endpoint
    if (!thermalData) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Error: Invalid data provided to sendImageData_Img (thermalData is null)."));
        #endif
        sysLed.setState(ERROR_DATA);
        ErrorLogger::sendLog(sdMgr, timeMgr, api_obj.getBaseApiUrl() + cfg.apiLogPath, api_obj.getAccessToken(), LOG_TYPE_ERROR, "sendImageData_Img called with null thermal data.", internalTempForLog);
        return false;
    }
    
    sysLed.setState(SENDING_DATA);

    String fullUrl = api_obj.getBaseApiUrl() + cfg.apiCaptureDataPath;
    String token = api_obj.getAccessToken();

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[ImgTasks] Preparing to send capture data via HTTP POST (multipart)..."));
    #endif

    // 1. Primer intento de envío
    // MultipartDataSender maneja internamente si jpegImage es nullptr
    int httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, timestamp, thermalData, jpegImage, jpegLength);

    if (httpCode >= 200 && httpCode < 300) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Capture data sent successfully."));
        #endif
        return true; // Éxito
    } 
    
    // 2. Manejo de error de autenticación (401 Unauthorized)
    if (httpCode == 401 && api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[ImgTasks] Capture data send failed (401). Attempting token refresh..."));
        #endif
        ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::WARNING, "Capture data send failed (401 Unauthorized). Attempting token refresh.", internalTempForLog);
        
        // Intenta refrescar el token
        int refreshHttpCode = api_obj.performTokenRefresh(); 
        
        if (refreshHttpCode == 200) { // Refresco exitoso
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[ImgTasks] Token refresh successful. Re-trying capture data send..."));
            #endif
            
            // 3. Segundo intento de envío (con el nuevo token)
            token = api_obj.getAccessToken();
            httpCode = MultipartDataSender::IOThermalAndImageData(fullUrl, token, timestamp, thermalData, jpegImage, jpegLength);
            
            if (httpCode >= 200 && httpCode < 300) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[ImgTasks] Capture data sent successfully on retry."));
                #endif
                return true; // Éxito en el reintento
            }
        }
    }

    // 4. Fallo final (después del reintento, o por error != 401)
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[ImgTasks] Error sending capture data. Final HTTP Code: %d\n", httpCode);
    #endif
    ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, "Error sending capture data. Final HTTP Code: " + String(httpCode), internalTempForLog);
    sysLed.setState(ERROR_SEND);
    return false;
}

/**
 * @brief Orquesta la captura, envío y archivo/guardado de los datos de imagen.
 */
bool performImageTasks_Img(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, OV2640Sensor& visCamera, MLX90640Sensor& thermalSensor, LEDStatus& sysLed, float lightLevel, uint8_t** jpegImage, size_t& jpegLength, float** thermalData, float internalTempForLog) { 

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("\n[ImgTasks] --- Performing Image Data Tasks (Capture, Send, Archive) ---"));
    #endif

    String timestamp = timeMgr.getCurrentTimestampString();
    *jpegImage = nullptr;
    jpegLength = 0;
    *thermalData = nullptr;
    
    sysLed.setState(TAKING_DATA);

    // --- 1. Decidir si capturar la imagen visual ---
    // (Basado en el nivel de luz medido por la tarea ambiental)
    bool captureVisual = (lightLevel >= RGB_CAPTURE_MIN_LIGHT_LEVEL_LUX);
    if (!captureVisual) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] Low light condition (%.2f lux). Skipping visual image capture.\n", lightLevel);
        #endif
    }

    // --- 2. Capturar Datos Térmicos (Mandatorio) ---
    if (!captureAndCopyThermalData_Img(sdMgr, timeMgr, thermalSensor, thermalData, cfg, api_obj, internalTempForLog)) {
        return false; // Falla crítica
    }

    // --- 3. Capturar Datos Visuales (Opcional) ---
    if (captureVisual) {
        if (!captureVisualJPEG_Img(sdMgr, timeMgr, visCamera, jpegImage, jpegLength, cfg, api_obj, internalTempForLog)) {
            // Falla la captura visual, pero la térmica tuvo éxito.
            // Limpiamos la térmica y retornamos false.
            if (*thermalData != nullptr) { free(*thermalData); *thermalData = nullptr; }
            return false;
        }
    }

    // --- 4. Intentar Enviar Datos ---
    // (Se envían los datos térmicos, y los visuales *si existen*)
    bool sentSuccessfully = sendImageData_Img(sdMgr, timeMgr, cfg, api_obj, timestamp, *jpegImage, jpegLength, *thermalData, sysLed, internalTempForLog);

    // --- 5. Guardar en SD (Archive o Pending) ---
    // Esto se hace *independientemente* de si los buffers se liberan después.
    if (sdMgr.isSDAvailable()) {
        String baseFilename = timeMgr.getCurrentTimestampString(true); // YYYYMMDD_HHMMSS
        // Decide el directorio de destino basado en el éxito del envío
        String targetDir = sentSuccessfully ? String(ARCHIVE_CAPTURES_DIR) : String(CAPTURE_PENDING_DIR);
        
        bool thermalWritten = false;
        bool visualWritten = false;

        // Guardar el JSON térmico (siempre)
        if (*thermalData) {
            String thermalJsonString = MultipartDataSender::createThermalJson(timestamp, *thermalData);
            if (!thermalJsonString.isEmpty()) {
                if(sdMgr.writeTextFile(targetDir + "/" + baseFilename + "_thermal.json", thermalJsonString)) {
                   thermalWritten = true;
                }
            }
        }
        
        // Guardar el JPEG visual (si se capturó)
        if (*jpegImage && jpegLength > 0) {
            if(sdMgr.writeBinaryFile(targetDir + "/" + baseFilename + "_visual.jpg", *jpegImage, jpegLength)) {
                visualWritten = true;
            }
        }
        
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ImgTasks] Attempted save to SD: Thermal %s, Visual %s. Target: %s\n", (thermalWritten ? "OK" : "FAIL"), (visualWritten ? "OK" : (captureVisual ? "FAIL" : "SKIPPED")), targetDir.c_str());
        #endif
        // Loguear el resultado del guardado en SD
        String sdLogMsg = "Capture data saved to SD. Target: " + targetDir + ". Thermal: " + (thermalWritten ? "OK" : "FAIL") + ". Visual: " + (visualWritten ? "OK" : (captureVisual ? "FAIL" : "SKIPPED"));
        LogLevel sdLogLevel = (thermalWritten || visualWritten) ? LogLevel::INFO : LogLevel::WARNING;
        ErrorLogger::logToSdOnly(sdMgr, timeMgr, sdLogLevel, sdLogMsg, internalTempForLog);
    } 
    
    // El resultado final de la tarea depende de si se ENVIÓ exitosamente.
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