#ifndef API_H
#define API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "SDManager.h"      // Requerido para la referencia _sdManagerRef
#include "nvs_flash.h"      // Para almacenamiento seguro de la clave AES
#include "nvs.h"            // Para almacenamiento seguro de la clave AES
#include "esp_random.h"     // Para generar la clave AES y el IV
#include "mbedtls/aes.h"    // Para encriptación
#include "mbedtls/gcm.h"    // Para encriptación (Modo GCM)
// Dependencias para logs (ErrorLogger) y configuración (ConfigManager)
#include "ErrorLogger.h"
#include "TimeManager.h"
#include "ConfigManager.h"

// Define el tamaño de la clave AES en bytes (32 = AES-256)
#define API_AES_KEY_SIZE 32

// Define la ruta del archivo de estado (definido en SDManager)
#define API_STATE_FILE_PATH SDManager::API_STATE_FILENAME

/**
 * @class API
 * @brief Gestiona la comunicación, autenticación y estado con el backend.
 *
 * Esta clase encapsula:
 * 1. La lógica para la activación del dispositivo.
 * 2. La autenticación (obtención de tokens de acceso/refresco).
 * 3. El refresco automático de tokens.
 * 4. El almacenamiento seguro (encriptado en SD) del estado de la API
 * (tokens, estado de activación, etc.).
 * 5. La gestión de la clave de encriptación (AES) en NVS.
 */
class API {
public:
    /**
     * @brief Constructor de la clase API.
     * Carga los datos persistentes (tokens, estado) desde la SD.
     *
     * @param sdManager Referencia a una instancia inicializada de SDManager.
     * @param base_url URL base de la API.
     * @param activate_path Ruta relativa para la activación.
     * @param auth_path Ruta relativa para la verificación de autenticación.
     * @param refresh_path Ruta relativa para el refresco de tokens.
     */
    API(SDManager& sdManager, 
        const String& base_url, 
        const String& activate_path, 
        const String& auth_path, 
        const String& refresh_path);

    /**
     * @brief Destructor.
     */
    ~API();

    // --- Métodos Públicos de Estado (Getters) ---
    bool isActivated() const;
    String getAccessToken() const;
    String getBaseApiUrl() const;
    int getDataCollectionTimeMinutes() const;

    // --- Métodos Públicos de Acción ---

    /**
     * @brief Intenta activar el dispositivo contra la API.
     * Envía deviceId, activationCode y MAC address.
     * Si tiene éxito (HTTP 200), parsea y guarda los tokens y
     * establece el estado como activado.
     *
     * @param deviceId ID del dispositivo (desde config).
     * @param activationCode Código de activación (desde config).
     * @return Código de estado HTTP (ej. 200, 401, 404) o código de error negativo.
     */
    int performActivation(const String& deviceId, const String& activationCode);

    /**
     * @brief Verifica el estado del backend y la validez del token de acceso.
     * Si el token no existe, intenta un refresco.
     * Si la API retorna 401 (token inválido), intenta un refresco.
     * Si el refresco falla críticamente (ej. 401 en refresh), desactiva el dispositivo.
     *
     * @return Código de estado HTTP (ej. 200 si OK, o el código del refresco).
     */
    int checkBackendAndAuth();

    /**
     * @brief Intenta refrescar los tokens usando el refreshToken guardado.
     * Si tiene éxito (HTTP 200), parsea y guarda los nuevos tokens.
     * Si falla (ej. 401), desactiva el dispositivo (estado crítico).
     *
     * @return Código de estado HTTP (ej. 200, 401).
     */
    int performTokenRefresh();

    /**
     * @brief Establece la dirección MAC del dispositivo (necesaria para la activación).
     * @param mac La dirección MAC obtenida de WiFiManager.
     */
    void setDeviceMAC(const String& mac);
    
private:
    /// Referencia al gestor de la SD (para guardar/leer estado).
    SDManager& _sdManagerRef; 

    // Rutas de la API
    String _apiBaseUrl;
    String _apiActivatePath;
    String _apiAuthPath;
    String _apiRefreshTokenPath;

    // Estado de la API (guardado en SD)
    String _accessToken;
    String _refreshToken;
    int _dataCollectionTimeMinutes;
    bool _activatedFlag;

    String _deviceMACAddress; // MAC (obtenida de WiFiManager)

    // Seguridad (Clave AES-256)
    /// Buffer para la clave AES (guardada en NVS).
    uint8_t _aesKey[API_AES_KEY_SIZE]; 
    /// Flag: true si la clave AES se cargó/generó exitosamente desde NVS.
    bool _aesKeyInitialized;

    /**
     * @brief (Helper) Carga el estado de la API desde la SD.
     * Es llamado por el constructor. Desencripta los datos si la clave
     * AES está inicializada y los datos parecen estar encriptados (Base64).
     * Si falla la carga o desencriptación, usa valores por defecto.
     */
    void _loadPersistentData();

    /**
     * @brief (Helper) Guarda el estado actual de la API en la SD.
     * Encripta los datos (JSON) usando AES-GCM, los codifica en Base64
     * y los pasa a `_sdManagerRef.saveApiState()`.
     * @return true si el guardado fue exitoso.
     */
    bool _saveCurrentApiStateToSd();

    /**
     * @brief (Helper) Inicializa la clave AES.
     * 1. Intenta cargar la clave desde NVS (almacenamiento seguro).
     * 2. Si no existe, genera una nueva clave aleatoria.
     * 3. Guarda la nueva clave en NVS.
     * @return true si la clave está lista para usar.
     */
    bool _initAesKey();

    // --- Métodos privados de actualización de estado ---
    // (Estos métodos actualizan la variable miembro y luego llaman a
    // _saveCurrentApiStateToSd() para persistir el cambio).
    void _updateAccessToken(const String& token);
    void _updateRefreshToken(const String& token);
    void _updateDataCollectionTime(int minutes);
    void _updateActivationStatus(bool activated);
    // ---

    /**
     * @brief (Helper) Ejecuta una petición HTTP POST genérica.
     * @param fullUrl URL completa del endpoint.
     * @param authorizationToken Token de acceso (si es necesario, sin el prefijo "Device ").
     * @param jsonPayload Payload JSON a enviar.
     * @param[out] responsePayload String donde se almacena la respuesta del servidor.
     * @return Código de estado HTTP (o código de error negativo del cliente).
     */
    int _httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload);
    
    /**
     * @brief (Helper) Parsea una respuesta JSON de autenticación (de /activate o /refresh).
     * Extrae y actualiza (usando los métodos _update) el access/refresh token
     * y el tiempo de colección.
     * @param jsonResponse El string JSON de respuesta del servidor.
     * @return true si los tokens fueron parseados y actualizados exitosamente.
     */
    bool _parseAndStoreAuthResponse(const String& jsonResponse); 
};

#endif // API_H