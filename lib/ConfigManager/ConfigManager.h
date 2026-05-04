#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h> 

// --- Estructura de Configuración ---
/**
 * @brief Almacena todos los parámetros de configuración de la aplicación.
 *
 * @note Estos valores actúan como valores por defecto si el archivo
 * /config.json no existe o una clave específica falta en él.
 */
struct Config {
    String wifi_ssid = "DEFAULT_SSID";
    String wifi_pass = "";
    int deviceId = 0;
    String activationCode = "";
    String apiBaseUrl = "";
    String apiActivatePath = "/api/device-api/activate";
    String apiAuthPath = "/api/device-api/auth";
    String apiRefreshTokenPath = "/api/device-api/refresh-token";
    String apiLogPath = "/api/device-api/log";
    String apiAmbientDataPath = "/api/device-api/ambient-data";
    String apiCaptureDataPath = "/api/device-api/capture-data";
    int data_interval_minutes = 30;
};

// Declara la instancia *global* 'config'.
// Esta variable será *definida* (creada) en el archivo ConfigManager.cpp
// y será accesible desde cualquier otro archivo que incluya este .h.
extern Config config; 

// --- Prototipos de Funciones del ConfigManager ---

/** * @brief Inicializa y monta el sistema de archivos LittleFS.
 * Intenta montar. Si falla, intenta formatear el sistema de archivos
 * y montarlo de nuevo.
 * @return True si el sistema de archivos está montado y listo.
 */
bool initFilesystem(); 

/** * @brief Carga la configuración desde un archivo JSON a la struct global 'config'.
 * @param filename El nombre (ruta) del archivo de configuración en LittleFS.
 * @return True si la carga fue exitosa, false si falló (ej. archivo no existe, JSON inválido).
 */
bool loadConfiguration(const char *filename);

/** * @brief Wrapper (atajo) para cargar el archivo de configuración por defecto.
 * Llama a loadConfiguration() con la ruta definida en CONFIG_FILENAME.
 * Si falla, la struct 'config' simplemente mantendrá sus valores por defecto.
 */
void loadConfigurationFromFile();

/** * @brief Guarda un string JSON en el archivo de configuración.
 * Sobrescribe completamente el archivo /config.json existente.
 * @param jsonString El string de configuración en formato JSON válido.
 * @return True si se guardó correctamente, false si falló la escritura.
 */
bool saveConfiguration(const String& jsonString);

#endif // CONFIG_MANAGER_H