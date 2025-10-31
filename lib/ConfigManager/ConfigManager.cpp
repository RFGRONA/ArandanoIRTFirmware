#include "ConfigManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
// Nota: LEDStatus.h no se incluye aquí ya que initFilesystem() fue modificado
// para no recibir el parámetro (basado en el .h actual).

// Definición (creación) de la instancia global 'config'.
Config config;

// Define el nombre y ruta del archivo de configuración en LittleFS.
#define CONFIG_FILENAME "/config.json"

/**
 * @brief Inicializa y monta el sistema de archivos LittleFS.
 */
bool initFilesystem() { 
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Initializing LittleFS filesystem...");
    #endif

    // Intenta montar el sistema de archivos. (false = no formatear si falla)
    if (!LittleFS.begin(false)) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] Warning: Initial LittleFS mount failed! Attempting to format...");
        #endif
        delay(1000); // Pequeña pausa antes de formatear

        // Si el primer intento falla, intenta formatear y montar de nuevo.
        if (!LittleFS.begin(true)) { // (true = formatear si el montaje falla)
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[ConfigMgr] CRITICAL ERROR: Formatting LittleFS failed! Check hardware/partition scheme.");
            #endif
            return false;
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[ConfigMgr] LittleFS formatted successfully. Filesystem is now empty.");
            #endif
        }
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] LittleFS mounted successfully.");
    #endif
    return true;
}

/**
 * @brief Carga la configuración desde el archivo por defecto.
 */
void loadConfigurationFromFile() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Loading configuration from default file: " + String(CONFIG_FILENAME));
    #endif

    if (!loadConfiguration(CONFIG_FILENAME)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] Warning: loadConfiguration failed or file not found. Using default config values.");
        #endif
        // No se necesita hacer más; 'config' ya contiene los valores
        // por defecto definidos en la struct (ConfigManager.h).
    }
}

/**
 * @brief Carga la configuración desde un archivo JSON a la struct global 'config'.
 */
bool loadConfiguration(const char *filename) {
    // Abre el archivo de configuración en modo lectura ("r")
    File configFile = LittleFS.open(filename, "r");
    if (!configFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[ConfigMgr] Warning: Failed to open config file: %s. Using defaults.\n", filename);
        #endif
        return false;
    }

    JsonDocument doc; 

    // Parsea el contenido del archivo JSON
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close(); // Cierra el archivo tan pronto como se termina de leer

    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print("[ConfigMgr] Warning: Failed to parse config file JSON: ");
            Serial.println(error.c_str());
            Serial.println("[ConfigMgr] Using default configuration values.");
        #endif
        return false;
    }

    // --- Extracción de valores ---
    // Se utiliza el operador '||' (o lógico) de ArduinoJson.
    // Intenta leer el valor de "wifi_ssid" del JSON.
    // Si la clave no existe en el JSON, utiliza el valor que ya está
    // en la variable (ej. config.wifi_ssid), que es el valor por defecto.
    config.wifi_ssid = doc["wifi_ssid"] | config.wifi_ssid;
    config.wifi_pass = doc["wifi_pass"] | config.wifi_pass;

    config.deviceId = doc["deviceId"] | config.deviceId;
    config.activationCode = doc["activationCode"] | config.activationCode;

    config.apiBaseUrl = doc["apiBaseUrl"] | config.apiBaseUrl;
    config.apiActivatePath = doc["apiActivatePath"] | config.apiActivatePath;
    config.apiAuthPath = doc["apiAuthPath"] | config.apiAuthPath;
    config.apiRefreshTokenPath = doc["apiRefreshTokenPath"] | config.apiRefreshTokenPath;
    config.apiLogPath = doc["apiLogPath"] | config.apiLogPath;
    config.apiAmbientDataPath = doc["apiAmbientDataPath"] | config.apiAmbientDataPath;
    config.apiCaptureDataPath = doc["apiCaptureDataPath"] | config.apiCaptureDataPath;

    config.data_interval_minutes = doc["data_interval_minutes"] | config.data_interval_minutes;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Configuration loaded successfully from file.");
        // (Los logs detallados de cada variable se omiten aquí por brevedad,
        // pero el código original los imprime si ENABLE_DEBUG_SERIAL está activo)
    #endif
    return true;
}

/**
 * @brief Guarda un string JSON como el nuevo archivo de configuración.
 */
bool saveConfiguration(const String& jsonString) {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[ConfigMgr] Abriendo archivo de configuración para escritura: " + String(CONFIG_FILENAME));
    #endif

    // Abre el archivo en modo escritura ("w"), lo que trunca (borra) el archivo si ya existe.
    File configFile = LittleFS.open(CONFIG_FILENAME, "w"); 
    if (!configFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] ERROR: No se pudo abrir el archivo de config para escritura.");
        #endif
        return false;
    }

    // Escribe el contenido del string JSON en el archivo
    size_t bytesWritten = configFile.print(jsonString);
    configFile.close();

    // Verifica que se haya escrito la cantidad correcta de bytes
    if (bytesWritten == jsonString.length()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] Configuración guardada exitosamente.");
        #endif
        return true;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[ConfigMgr] ERROR: Falla al escribir en el archivo de config.");
        #endif
        return false;
    }
}