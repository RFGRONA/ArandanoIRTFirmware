#include "CycleController.h"
#include <LittleFS.h>     
#include "nvs_flash.h" 
#include "ErrorLogger.h" // Para registrar los resultados de la autenticación

/**
 * @brief Implementación de la espera de conexión WiFi (bloqueante).
 */
bool ensureWiFiConnected_Ctrl(WiFiManager& wifiMgr, LEDStatus& sysLed, unsigned long timeoutMs) {
    // Si ya está conectado, salir inmediatamente.
    if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
        return true;
    }

    // Si no está conectado, inicia el intento (si no se está conectando ya)
    if (wifiMgr.getConnectionStatus() != WiFiManager::CONNECTING) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[Ctrl] WiFi is not connected. Initiating connection attempt...");
        #endif
        wifiMgr.connectToWiFi();
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[Ctrl] WiFi connection already in progress...");
         #endif
    }

    sysLed.setState(CONNECTING_WIFI);
    unsigned long startTime = millis();

    // Bucle de espera (bloqueante) con timeout
    while (millis() - startTime < timeoutMs) {
        // Permite que el WiFiManager (que maneja eventos) se ejecute
        wifiMgr.handleWiFi(); 

        if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[Ctrl] WiFi connection established successfully within timeout.");
            #endif
            // El 'main loop' que llamó a esta función se encargará
            // de establecer el LED a ALL_OK.
            return true;
        }
        // Si WiFiManager reporta un fallo permanente (max reintentos), salimos
        if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTION_FAILED) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[Ctrl] WiFi connection failed permanently (max retries reached by WiFiManager).");
            #endif
            break; 
        }
        delay(100); 
    }

    // Si salimos del bucle, es por timeout o fallo permanente
    #ifdef ENABLE_DEBUG_SERIAL
        if (wifiMgr.getConnectionStatus() != WiFiManager::CONNECTION_FAILED) {
             Serial.printf("[Ctrl] WiFi connection attempt timed out after %lu ms.\n", timeoutMs);
        }
    #endif
    sysLed.setState(ERROR_WIFI);
    return false;
}

/**
 * @brief Implementación del parpadeo de LED.
 * Secuencia: OK -> OFF -> OK -> OFF -> OK -> OFF
 */
void ledBlink_Ctrl(LEDStatus& sysLed) {
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
    sysLed.setState(ALL_OK); delay(350);
    sysLed.setState(OFF);    delay(150);
}

/**
 * @brief Implementación de la lógica de autenticación y activación API.
 */
bool handleApiAuthenticationAndActivation_Ctrl(SDManager& sdMgr, TimeManager& timeMgr, Config& cfg, API& api_obj, LEDStatus& status_led, float internalTempForLog) { 
    String logUrl = api_obj.getBaseApiUrl() + cfg.apiLogPath;

    // --- 1. Activación (Solo si no está activado) ---
    if (!api_obj.isActivated()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl_API] Device not activated. Attempting activation..."));
        #endif
        status_led.setState(CONNECTING_WIFI); // Indica actividad de red

        int activationHttpCode = api_obj.performActivation(String(cfg.deviceId), cfg.activationCode);

        if (activationHttpCode == 200) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[Ctrl_API] Activation successful (HTTP 200)."));
            #endif
            // Registra el éxito
            ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_INFO, "Device activated successfully. DeviceID: " + String(cfg.deviceId), internalTempForLog);
            // (La activación fue exitosa, api_obj.isActivated() ahora es true)
        } else {
            // Fallo en la activación
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[Ctrl_API] Activation failed. HTTP Code: %d\n", activationHttpCode);
            #endif
            status_led.setState(ERROR_AUTH);
            ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, "", LOG_TYPE_ERROR, "Device activation failed. HTTP Code: " + String(activationHttpCode) + ", DeviceID: " + String(cfg.deviceId), internalTempForLog);
            return false; // No se puede continuar si la activación falla
        }
    }

    // --- 2. Verificación de Backend y Autenticación (Refresco de Token) ---
    // (Esto se ejecuta si ya estaba activado, o si la activación acaba de ser exitosa)
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl_API] Device activated. Performing backend and auth check..."));
    #endif
    status_led.setState(CONNECTING_WIFI); // Indica actividad de red

    int authHttpCode = api_obj.checkBackendAndAuth();

    if (authHttpCode == 200) {
        // Éxito (El token está válido o se refrescó correctamente)
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl_API] Backend & Auth check successful (HTTP 200)."));
        #endif
        return true;
    } else {
        // Fallo (No se pudo refrescar el token, o el backend está caído)
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[Ctrl_API] Backend & Auth check failed. HTTP Code: %d\n", authHttpCode);
        #endif
        status_led.setState(ERROR_AUTH); 

        String errorMessage = "Backend/Auth check failed. HTTP Code: " + String(authHttpCode);
        // api_obj.checkBackendAndAuth puede desactivar el dispositivo si el
        // refresh token falla permanentemente.
        if (!api_obj.isActivated()) { 
            errorMessage += ". Device has been deactivated.";
        }
        ErrorLogger::sendLog(sdMgr, timeMgr, logUrl, api_obj.getAccessToken(), LOG_TYPE_ERROR, errorMessage, internalTempForLog);
        return false;
    }
}


/**
 * @brief Implementación de la limpieza de buffers de imagen.
 */
void cleanupImageBuffers_Ctrl(uint8_t* jpegImage, float* thermalData) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] --- Cleaning Up Image Buffers ---"));
    #endif

    // Libera el buffer de la imagen JPEG (si existe)
    if (jpegImage != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl] Freeing JPEG image buffer..."));
        #endif
        free(jpegImage);
    }

    // Libera el buffer de datos térmicos (si existe)
    if (thermalData != nullptr) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[Ctrl] Freeing thermal data buffer..."));
        #endif
        free(thermalData);
    }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[Ctrl] Image buffers freed."));
    #endif
}