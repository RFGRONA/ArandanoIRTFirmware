#include "SystemInit.h"
#include <Wire.h>         // Para inicialización I2C
#include <LittleFS.h>     // Para LittleFS.end() en el manejador de fallos
#include "ErrorLogger.h" // Para registro de errores

// --- Definiciones para rutinas robustas de inicio (Setup) ---

///< Máximos reintentos para la conexión WiFi en el setup.
#define WIFI_SETUP_MAX_RETRIES 5
///< Retardo inicial (segundos) entre reintentos de WiFi.
#define WIFI_SETUP_INITIAL_BACKOFF_S 4  
///< Máximo retardo (segundos) entre reintentos de WiFi (tope de backoff).
#define WIFI_SETUP_MAX_BACKOFF_S 30     

///< Máximos reintentos para la sincronización NTP en el setup.
#define NTP_SETUP_MAX_RETRIES 5
///< Retardo (ms) entre reintentos de NTP.
#define NTP_SETUP_RETRY_DELAY_MS 2000
///< Tiempo que el sistema se detiene antes de un reinicio forzado (1 hora).
#define SYSTEM_HALT_DELAY_MS 3600000UL 


/**
 * @brief Inicializa el Serial (si está habilitado) y muestra mensaje de arranque.
 */
void initSerial_Sys() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.begin(115200);
        delay(1000); // Dar tiempo a que el monitor serie se conecte
        uint64_t chipid = ESP.getEfuseMac();
        // (Salidas de terminal mantenidas en inglés)
        Serial.printf("\n--- Device Booting / Waking Up (Chip ID: %04X%08X) ---\n",
                      (uint16_t)(chipid >> 32),
                      (uint32_t)chipid);
        Serial.println("[SysInit] Debug Serial Enabled (Rate: 115200)");
    #endif
}

/**
 * @brief Inicializa el bus I2C (Wire).
 */
void initI2C_Sys(int sdaPin, int sclPin, uint32_t frequency) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SysInit] Initializing I2C Bus (Pins SDA: %d, SCL: %d, Freq: %lu Hz)...\n", sdaPin, sclPin, frequency);
    #endif
    Wire.begin(sdaPin, sclPin);
    Wire.setClock(frequency);
    delay(100); // Pequeña pausa después de iniciar I2C
}


/**
 * @brief Inicializa todos los sensores en secuencia.
 * @return String vacío si OK, o lista de sensores fallidos.
 */
String initializeSensors_Sys(BME280Sensor& bme, BH1750Sensor& light, MLX90640Sensor& thermal, OV2640Sensor& visCamera) {
    String failures = ""; // Acumulador de fallos
    bool allOk = true;

    // --- Inicializar BME280 (Temp/Hum/Pres) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing BME280 sensor...");
    #endif
    if (!bme.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! BME280 Sensor Initialization FAILED !!!");
        #endif
        failures += "BME280,";
        allOk = false;
    }
    delay(100);

    // --- Inicializar BH1750 (Luz) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing BH1750 (Light Sensor)...");
    #endif
    if (!light.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! BH1750 Light Sensor Initialization FAILED !!!");
        #endif
        failures += "BH1750,";
        allOk = false;
    }
    delay(100);

    // --- Inicializar MLX90640 (Térmica) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing MLX90640 (Thermal Sensor)...");
    #endif
    if (!thermal.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! MLX90640 Thermal Sensor Initialization FAILED !!!");
        #endif
        failures += "MLX90640,";
        allOk = false;
    }
    
    // Espera crítica para la estabilización de la primera medición del MLX90640.
    // Requerido por la librería/datasheet (depende del refresh rate configurado).
    #ifdef ENABLE_DEBUG_SERIAL
        if (allOk) {
            Serial.println("[SysInit] Waiting for MLX90640 measurement stabilization (~2 seconds)...");
        }
    #endif
    delay(2000); 

    // --- Inicializar OV2640 (Visual) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Initializing OV2640 (Visual Camera)...");
    #endif
    if (!visCamera.begin()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] !!! OV2640 Camera Initialization FAILED !!!");
        #endif
        failures += "OV2640,";
        allOk = false;
    }
    delay(500);

    if (allOk) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SysInit] All sensors initialized successfully.");
        #endif
        return ""; // Retorna string vacío si todo OK
    } else {
        // Limpia la última coma (ej. "BME280,MLX90640,")
        if (failures.endsWith(",")) {
            failures.remove(failures.length() - 1);
        }
        return failures; // Retorna la lista de fallos
    }
}

/**
 * @brief Manejador de fallo crítico de sensores. Detiene la ejecución.
 */
void handleSensorInitFailure_Sys(SDManager& sdManager, TimeManager& timeManager, const String& failedSensors) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit] CRITICAL ERROR: Sensor initialization failed. Halting execution."));
        Serial.printf("[SysInit] Failed Sensors: %s\n", failedSensors.c_str()); 
    #endif

    // Registra el error fatal en la SD (si es posible)
    String logMsg = "CRITICAL: Sensor init failed. Failed: [" + failedSensors + "]. System halted.";
    ErrorLogger::logToSdOnly(sdManager, timeManager, LogLevel::ERROR, logMsg);

    // Desmonta el sistema de archivos de forma segura
    LittleFS.end();
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit] Unmounted LittleFS.");
    #endif
    delay(500);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- SYSTEM HALTED ---"));
        Serial.flush();
    #endif
    // Detiene la ejecución. Un Watchdog (WDT) eventualmente reiniciará el dispositivo.
    while(1) {
        delay(1000);
    }
}

/**
 * @brief Inicializa WiFi (bloqueante, con reintentos y backoff).
 * @return true si se conecta, false si falla todos los reintentos.
 */
bool initializeWiFi_Sys(WiFiManager& wifiMgr, LEDStatus& led, Config& cfg, API* api_comm, 
                        SDManager& sdMgr, TimeManager& timeMgr, OV2640Sensor& visCamera) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit_WiFi] Initializing WiFiManager and setting credentials..."));
    #endif
    wifiMgr.begin();
    wifiMgr.setCredentials(cfg.wifi_ssid, cfg.wifi_pass);

    // Intenta establecer la MAC en el objeto API (necesario para la activación)
    if (api_comm != nullptr) { 
        String macAddr = wifiMgr.getMacAddress();
        if (!macAddr.isEmpty()) {
            api_comm->setDeviceMAC(macAddr);
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_WiFi] MAC Address %s set in API object.\n", macAddr.c_str());
            #endif
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println(F("[SysInit_WiFi] WARNING: Could not obtain MAC address for API object."));
            #endif
            ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::WARNING, "Could not obtain MAC address for API object.");
        }
    }

    led.setState(CONNECTING_WIFI);
    unsigned long backoffDelayS = WIFI_SETUP_INITIAL_BACKOFF_S;
    
    // Bucle de reintentos de conexión
    for (int attempt = 1; attempt <= WIFI_SETUP_MAX_RETRIES; ++attempt) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_WiFi] Connection attempt #%d/%d...\n", attempt, WIFI_SETUP_MAX_RETRIES);
        #endif
        
        wifiMgr.connectToWiFi(); // Inicia el intento

        // Bucle de espera (timeout) para este intento
        unsigned long attemptStartTime = millis();
        while (millis() - attemptStartTime < WIFI_CONNECT_TIMEOUT) {
            if (wifiMgr.getConnectionStatus() == WiFiManager::CONNECTED) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println(F("[SysInit_WiFi] WiFi connected successfully."));
                #endif
                led.setState(ALL_OK);
                return true; // Éxito
            }
            delay(100);
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_WiFi] Attempt #%d timed out after %d ms.\n", attempt, WIFI_CONNECT_TIMEOUT);
        #endif

        // Si no es el último intento, espera con "backoff"
        if (attempt < WIFI_SETUP_MAX_RETRIES) {
            led.setState(ERROR_WIFI);
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_WiFi] Waiting for %lu seconds before retrying...\n", backoffDelayS);
            #endif
            delay(backoffDelayS * 1000);
            // Incrementa el backoff (duplica, con un tope)
            backoffDelayS = min(backoffDelayS * 2, (unsigned long)WIFI_SETUP_MAX_BACKOFF_S); 
            led.setState(CONNECTING_WIFI);
        }
    }
    
    // Si salimos del bucle, todos los reintentos fallaron
    String errorMsg = "CRITICAL: All WiFi connection attempts failed. Returning to main loop."; 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit_WiFi] " + errorMsg);
    #endif
    led.setState(ERROR_WIFI);
    ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, errorMsg);

    return false; // Fallo
}


/**
 * @brief Sincroniza NTP (bloqueante, con reintentos).
 * @return true si se sincroniza. (Falla = reinicia el dispositivo).
 */
bool initializeNTP_Sys(TimeManager& timeMgr, SDManager& sdMgr, API* api_comm, Config& cfg,
                       long gmtOffset_sec, int daylightOffset_sec) {
    // Pre-condición: WiFi debe estar conectado.
    if (WiFi.status() != WL_CONNECTED) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SysInit_NTP] FATAL: WiFi not connected. Cannot sync NTP. Halting."));
         #endif
         ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, "FATAL: WiFi not connected. Cannot sync NTP. Halting.");
        delay(SYSTEM_HALT_DELAY_MS); // Espera 1 hora
        ESP.restart();
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SysInit_NTP] Initializing TimeManager and starting NTP sync..."));
    #endif
    timeMgr.begin(DEFAULT_NTP_SERVER_1, DEFAULT_NTP_SERVER_2, gmtOffset_sec, daylightOffset_sec);
    
    // Bucle de reintentos de sincronización
    for (int attempt = 1; attempt <= NTP_SETUP_MAX_RETRIES; ++attempt) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_NTP] NTP sync attempt #%d/%d...\n", attempt, NTP_SETUP_MAX_RETRIES);
        #endif
        
        // timeMgr.syncNtpTime() es bloqueante
        if (timeMgr.syncNtpTime()) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SysInit_NTP] NTP time synchronized: " + timeMgr.getCurrentTimestampString());
            #endif
            // Loguea el éxito solo a la SD (la API puede necesitar la hora)
            ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::INFO, "NTP time synchronized successfully at setup.");
            return true; // Éxito
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SysInit_NTP] NTP sync attempt #%d failed.\n", attempt);
        #endif

        if (attempt < NTP_SETUP_MAX_RETRIES) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SysInit_NTP] Waiting for %d ms before retrying...\n", NTP_SETUP_RETRY_DELAY_MS);
            #endif
            delay(NTP_SETUP_RETRY_DELAY_MS);
        }
    }

    // Si salimos del bucle, todos los reintentos fallaron (Error fatal)
    String errorMsg = "CRITICAL: All NTP time synchronization attempts failed. Halting.";
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SysInit_NTP] " + errorMsg);
    #endif
    // El timestamp del log será basado en UPTIME, ya que NTP falló.
    ErrorLogger::logToSdOnly(sdMgr, timeMgr, LogLevel::ERROR, errorMsg);

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("--- SYSTEM HALTED ---"));
        Serial.flush();
    #endif
    delay(SYSTEM_HALT_DELAY_MS); // Espera 1 hora
    ESP.restart(); // Fuerza el reinicio
    
    return false; // (No se alcanza)
}