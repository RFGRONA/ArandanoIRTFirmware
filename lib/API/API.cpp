#include "API.h"
#include "nvs_flash.h"      // Para NVS
#include "nvs.h"            // Para NVS
#include "FS.h"
#include "SD_MMC.h"
#include "esp_random.h"     // Para esp_fill_random (generar clave y IV)
#include "mbedtls/aes.h" 
#include "mbedtls/gcm.h"    // Para AES-GCM (encriptación autenticada)
#include "mbedtls/base64.h" // Para codificar/decodificar el estado guardado
// (Dependencias adicionales)
#include "ErrorLogger.h"
#include "TimeManager.h"
#include "ConfigManager.h"
#include "EnvironmentDataJSON.h"
#include "MultipartDataSender.h"

///< Timeout estándar para peticiones HTTP de la API.
#define HTTP_REQUEST_TIMEOUT 10000

// --- Constantes para Encriptación AES-GCM ---
#define AES_GCM_IV_LENGTH 12  // 12 bytes (96 bits) es el estándar recomendado
#define AES_GCM_TAG_LENGTH 16 // 16 bytes (128 bits) para autenticación

// --- Claves para el JSON de estado (guardado en SD) ---
const char* JSON_KEY_ACCESS_TOKEN = "accessToken";
const char* JSON_KEY_REFRESH_TOKEN = "refreshToken";
const char* JSON_KEY_COLLECTION_TIME = "dataCollectionTime";
const char* JSON_KEY_IS_ACTIVATED = "isActivated";

// --- Definiciones NVS (para la clave AES) ---
#define NVS_NAMESPACE "api_secure"  // Espacio de nombres en NVS
#define NVS_AES_KEY_NAME "aes_key"  // Nombre de la clave en NVS

/**
 * @brief Constructor.
 */
API::API(SDManager& sdManager, const String& base_url, const String& activate_path, const String& auth_path, const String& refresh_path) :
    _sdManagerRef(sdManager), 
    _apiBaseUrl(base_url),
    _apiActivatePath(activate_path),
    _apiAuthPath(auth_path),
    _apiRefreshTokenPath(refresh_path),
    _dataCollectionTimeMinutes(0),
    _activatedFlag(false),
    _deviceMACAddress(""),
    _aesKeyInitialized(false) {
    
    // 1. Inicializa (carga/crea) la clave de encriptación AES desde NVS.
    if (!_initAesKey()) {
        #ifdef ENABLE_DEBUG_SERIAL
            // (Salida de terminal en inglés)
            Serial.println(F("[API CRITICAL] AES Key initialization FAILED. API state will not be encrypted/decrypted."));
        #endif
    }
    // 2. Carga el estado (tokens, etc.) desde la SD (y desencripta si es posible).
    _loadPersistentData();
}

API::~API() {
    // (Destructor vacío)
}

/**
 * @brief Establece la MAC (obtenida de WiFiManager).
 */
void API::setDeviceMAC(const String& mac) {
    _deviceMACAddress = mac;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[API] Device MAC address set to: %s\n", _deviceMACAddress.c_str());
    #endif
}

/**
 * @brief (Helper) Guarda el estado actual de la API (encriptado) en la SD.
 */
bool API::_saveCurrentApiStateToSd() {
    if (!_sdManagerRef.isSDAvailable()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] SD card not available. Cannot save API state.");
        #endif
        return false;
    }

    // 1. Serializar el estado actual a un JSON (texto plano)
    JsonDocument doc;
    doc[JSON_KEY_ACCESS_TOKEN] = _accessToken;
    doc[JSON_KEY_REFRESH_TOKEN] = _refreshToken;
    doc[JSON_KEY_COLLECTION_TIME] = _dataCollectionTimeMinutes;
    doc[JSON_KEY_IS_ACTIVATED] = _activatedFlag;

    String stateJsonToSavePlain;
    serializeJson(doc, stateJsonToSavePlain);

    if (stateJsonToSavePlain.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] Failed to serialize API state to JSON for encryption.");
        #endif
        return false;
    }

    String dataToStoreOnSd; 

    // 2. Encriptar (si la clave AES está lista)
    if (_aesKeyInitialized) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] Encrypting API state...");
        #endif
        
        mbedtls_gcm_context gcm;
        unsigned char iv[AES_GCM_IV_LENGTH];      // Vector de inicialización (aleatorio)
        unsigned char tag[AES_GCM_TAG_LENGTH];  // Etiqueta de autenticación (generada)
        
        size_t plaintextLength = stateJsonToSavePlain.length();
        // Buffer para el texto cifrado (mismo tamaño que el texto plano)
        unsigned char* ciphertextBuffer = (unsigned char*)malloc(plaintextLength);
        if (!ciphertextBuffer) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Save] Failed to allocate memory for ciphertext buffer!");
            #endif
            return false;
        }

        // Generar un IV aleatorio para cada encriptación
        esp_fill_random(iv, AES_GCM_IV_LENGTH);

        // Configurar GCM con la clave
        mbedtls_gcm_init(&gcm);
        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _aesKey, API_AES_KEY_SIZE * 8); // 32 bytes * 8 = 256 bits
        if (ret != 0) {
            mbedtls_gcm_free(&gcm); free(ciphertextBuffer); return false;
        }

        // Encriptar y generar la etiqueta (tag)
        ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintextLength, 
                                        iv, AES_GCM_IV_LENGTH, NULL, 0, // (Sin datos adicionales autenticados)
                                        (const unsigned char*)stateJsonToSavePlain.c_str(), ciphertextBuffer,
                                        AES_GCM_TAG_LENGTH, tag);
        mbedtls_gcm_free(&gcm);
        if (ret != 0) {
            free(ciphertextBuffer); return false;
        }

        // 3. Combinar [IV] + [TAG] + [Ciphertext] en un solo buffer
        size_t totalEncryptedLength = AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH + plaintextLength;
        std::vector<unsigned char> combinedBuffer(totalEncryptedLength);
        memcpy(combinedBuffer.data(), iv, AES_GCM_IV_LENGTH);
        memcpy(combinedBuffer.data() + AES_GCM_IV_LENGTH, tag, AES_GCM_TAG_LENGTH);
        memcpy(combinedBuffer.data() + AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH, ciphertextBuffer, plaintextLength);
        
        free(ciphertextBuffer);

        // 4. Codificar el buffer combinado en Base64
        size_t base64Len;
        mbedtls_base64_encode(NULL, 0, &base64Len, combinedBuffer.data(), combinedBuffer.size()); // Calcular tamaño
        std::vector<unsigned char> base64Buffer(base64Len);
        
        ret = mbedtls_base64_encode(base64Buffer.data(), base64Len, &base64Len, combinedBuffer.data(), combinedBuffer.size());
        if(ret != 0){ return false; }

        dataToStoreOnSd = String((char*)base64Buffer.data());
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] API state encrypted and Base64 encoded successfully.");
        #endif

    } else {
        // Fallback: Si AES falló, guardar como texto plano (no ideal)
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] AES key not initialized. Saving API state as PLAINTEXT.");
        #endif
        dataToStoreOnSd = stateJsonToSavePlain;
    }

    // 5. Guardar en la SD
    bool success = _sdManagerRef.saveApiState(dataToStoreOnSd);
    return success;
}

/**
 * @brief (Helper) Carga y desencripta el estado de la API desde la SD.
 */
void API::_loadPersistentData() {
    String stateJsonFromSdBase64; 
    // 1. Intentar leer el archivo de estado de la SD
    if (_sdManagerRef.isSDAvailable() && _sdManagerRef.readApiState(stateJsonFromSdBase64) && !stateJsonFromSdBase64.isEmpty()) {
        
        String stateJsonPlain = ""; 

        // 2. Verificar si la clave AES está lista y si los datos *parecen* encriptados (Base64)
        if (_aesKeyInitialized && stateJsonFromSdBase64.length() > 50) { // (Chequeo simple de longitud)
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Load] Data found on SD. Attempting decryption...");
            #endif
            
            // 3. Decodificar Base64
            size_t decodedLen;
            mbedtls_base64_decode(NULL, 0, &decodedLen, (const unsigned char*)stateJsonFromSdBase64.c_str(), stateJsonFromSdBase64.length()); // Calcular tamaño
            std::vector<unsigned char> encryptedBuffer(decodedLen);
            
            int ret = mbedtls_base64_decode(encryptedBuffer.data(), encryptedBuffer.size(), &decodedLen, (const unsigned char*)stateJsonFromSdBase64.c_str(), stateJsonFromSdBase64.length());

            // Verificar si la decodificación Base64 falló o es demasiado corta
            if (ret != 0 || decodedLen < (AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH)) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Base64 decode failed (-0x%04X) or decoded data too short. Treating as corrupt.\n", -ret);
                #endif
                goto use_defaults; // Saltar a la sección de usar valores por defecto
            }

            // 4. Extraer [IV], [TAG] y [Ciphertext] del buffer combinado
            unsigned char iv[AES_GCM_IV_LENGTH];
            unsigned char tag[AES_GCM_TAG_LENGTH];
            size_t ciphertextLength = decodedLen - AES_GCM_IV_LENGTH - AES_GCM_TAG_LENGTH;
            
            memcpy(iv, encryptedBuffer.data(), AES_GCM_IV_LENGTH);
            memcpy(tag, encryptedBuffer.data() + AES_GCM_IV_LENGTH, AES_GCM_TAG_LENGTH);
            
            std::vector<unsigned char> plaintextBuffer(ciphertextLength);

            // 5. Desencriptar y Autenticar (AES-GCM)
            mbedtls_gcm_context gcm;
            mbedtls_gcm_init(&gcm);
            ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _aesKey, API_AES_KEY_SIZE * 8);
            if (ret != 0) { mbedtls_gcm_free(&gcm); goto use_defaults; }

            ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLength,
                                           iv, AES_GCM_IV_LENGTH, NULL, 0, // (Sin AAD)
                                           tag, AES_GCM_TAG_LENGTH,
                                           encryptedBuffer.data() + AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH, // Puntero al inicio del ciphertext
                                           plaintextBuffer.data());
            mbedtls_gcm_free(&gcm);

            if (ret == 0) { // ¡Éxito!
                stateJsonPlain = String((char*)plaintextBuffer.data(), plaintextBuffer.size());
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[API_Load] API state decrypted successfully!");
                #endif
            } else {
                // Fallo de autenticación (Tag incorrecto) o desencriptación
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Decryption FAILED (mbedtls_gcm_auth_decrypt: -0x%04X). Data is corrupt or key is wrong.\n", -ret);
                #endif
                goto use_defaults;
            }

        } else {
            // Cargar como Texto Plano (si la clave AES falló o los datos son cortos)
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Load] Loading API state as PLAINTEXT (key not ready or data seems unencrypted).");
            #endif
            stateJsonPlain = stateJsonFromSdBase64;
        }

        // 6. Parsear el JSON (ya sea desencriptado o de texto plano)
        if (!stateJsonPlain.isEmpty()) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, stateJsonPlain);
            if (error) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Failed to parse API state JSON: %s. Using defaults.\n", error.c_str());
                #endif
                goto use_defaults;
            } else {
                // Cargar estado desde el JSON
                _accessToken = doc[JSON_KEY_ACCESS_TOKEN] | "";
                _refreshToken = doc[JSON_KEY_REFRESH_TOKEN] | "";
                _dataCollectionTimeMinutes = doc[JSON_KEY_COLLECTION_TIME] | 0;
                _activatedFlag = doc[JSON_KEY_IS_ACTIVATED] | false;
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[API_Load] API state parsed successfully.");
                #endif
            }
        } else {
             goto use_defaults;
        }

    } else { 
        // 7. Usar valores por defecto (si el archivo no existe o está vacío)
use_defaults:
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Load] No valid API state found. Using defaults and saving a new state file.");
        #endif
        _accessToken = "";
        _refreshToken = "";
        _dataCollectionTimeMinutes = 0;
        _activatedFlag = false;
        // Intenta guardar este estado por defecto (encriptado si es posible)
        if (_sdManagerRef.isSDAvailable()) {
             _saveCurrentApiStateToSd();
        }
    }

    if (_dataCollectionTimeMinutes <= 0) { _dataCollectionTimeMinutes = 0; }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[API_Load] Effective State after load: Activated: %s\n", _activatedFlag ? "Yes" : "No");
    #endif
}


// --- Métodos Privados de Actualización (Setters) ---
// (Estos métodos llaman a _saveCurrentApiStateToSd() para persistir)

void API::_updateAccessToken(const String& token) {
    _accessToken = token;
    _saveCurrentApiStateToSd();
}

void API::_updateRefreshToken(const String& token) {
    _refreshToken = token;
    _saveCurrentApiStateToSd();
}

void API::_updateDataCollectionTime(int minutes) {
    if (minutes <= 0) minutes = 0;
    _dataCollectionTimeMinutes = minutes;
    _saveCurrentApiStateToSd();
}

void API::_updateActivationStatus(bool activated) {
    _activatedFlag = activated;
    _saveCurrentApiStateToSd();
}

// --- Métodos Públicos de Estado (Getters) ---
bool API::isActivated() const { return _activatedFlag; }
String API::getAccessToken() const { return _accessToken; }
String API::getBaseApiUrl() const { return _apiBaseUrl; }
int API::getDataCollectionTimeMinutes() const {
    return _dataCollectionTimeMinutes > 0 ? _dataCollectionTimeMinutes : 0;
}

// --- Métodos Centrales de Interacción API ---

/**
 * @brief (Helper) Ejecuta un POST HTTP.
 */
int API::_httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload) {
    HTTPClient http;
    int httpResponseCode = -1; // Error genérico

    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_httpPost] No WiFi connection.");
        #endif
        return -100; // Código de error: Sin WiFi
    }
    
    http.setReuse(false); 
    if (http.begin(fullUrl)) {
        http.setTimeout(HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Connection", "close"); 
        // Añadir token de autorización (si se proporciona)
        if (!authorizationToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + authorizationToken);
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_httpPost] POST to: %s\n", fullUrl.c_str());
        #endif

        // Maneja POST con o sin payload
        if(jsonPayload.isEmpty()){
             httpResponseCode = http.POST("");
        } else {
             httpResponseCode = http.POST(jsonPayload);
        }

        if (httpResponseCode > 0) {
            responsePayload = http.getString();
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_httpPost] Response Code: %d\n", httpResponseCode);
                Serial.printf("[API_httpPost] Response Payload: %s\n", responsePayload.c_str());
            #endif
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_httpPost] Request failed, error: %s (Code: %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
            #endif
        }
        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_httpPost] Unable to begin connection to %s\n", fullUrl.c_str());
        #endif
        httpResponseCode = -101; // Código de error: http.begin() falló
    }
    return httpResponseCode;
}

/**
 * @brief (Helper) Parsea la respuesta de /activate o /refresh y guarda los tokens.
 */
bool API::_parseAndStoreAuthResponse(const String& jsonResponse) {
    if (jsonResponse.isEmpty()) return false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonResponse);

    if (error) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(F("[API_parse] JSON deserialization failed for auth response: "));
            Serial.println(error.f_str());
        #endif
        return false;
    }

    const char* newAccessToken = doc[JSON_KEY_ACCESS_TOKEN];
    const char* newRefreshToken = doc[JSON_KEY_REFRESH_TOKEN];
    int newDataCollectionTime = doc[JSON_KEY_COLLECTION_TIME] | 0; 

    bool tokensProcessed = false;
    // Ambos tokens son requeridos
    if (newAccessToken && newRefreshToken) {
        // Llama a los métodos _update, que guardarán en la SD
        _updateAccessToken(String(newAccessToken)); 
        _updateRefreshToken(String(newRefreshToken)); 
        tokensProcessed = true;
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_parse] AccessToken and RefreshToken updated from response."));
        #endif
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_parse] accessToken or refreshToken missing in auth JSON response."));
        #endif
    }
    
    // Actualiza el tiempo de colección si es > 0
    if (newDataCollectionTime > 0) {
        _updateDataCollectionTime(newDataCollectionTime); 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_parse] DataCollectionTimeMinutes updated to: %d via _update method.\n", _dataCollectionTimeMinutes);
        #endif
    }
    
    // Retorna true solo si los tokens (lo esencial) se procesaron
    return tokensProcessed; 
}

/**
 * @brief Intenta la activación del dispositivo.
 */
int API::performActivation(const String& deviceId, const String& activationCode) {
    if (deviceId.isEmpty() || activationCode.isEmpty()) return -102; // Error: Datos faltantes

    // 1. Construir payload de activación
    JsonDocument doc;
    doc["deviceId"] = deviceId.toInt();
    doc["activationCode"] = activationCode;
    if (!_deviceMACAddress.isEmpty()) { 
        doc["macAddress"] = _deviceMACAddress;
    }

    String payload;
    serializeJson(doc, payload);
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiActivatePath;

    // 2. Realizar POST
    int httpCode = _httpPost(fullUrl, "", payload, responsePayload); // Sin token de autorización

    // 3. Procesar respuesta
    if (httpCode == 200) {
        // Éxito: Parsear y guardar los tokens recibidos
        if (_parseAndStoreAuthResponse(responsePayload)) {
            _updateActivationStatus(true); // Marca como activado y guarda
        } else {
            // Error: HTTP 200 pero el JSON de respuesta está corrupto o no tiene tokens
            httpCode = -103; // Código de error: Fallo de parseo
            _updateActivationStatus(false);
        }
    } else {
        // Fallo (401, 404, 500, etc.)
        _updateActivationStatus(false);
    }
    return httpCode;
}

/**
 * @brief Verifica el estado del backend y la validez del token de acceso actual.
 */
int API::checkBackendAndAuth() {
    // 1. No intentar si no está activado
    if (!_activatedFlag) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Device not activated. Skipping auth check."));
        #endif
        return -104; // Error: No activado
    }
    
    // 2. Si no tenemos AccessToken (ej. recién activado o reinicio), refrescar primero
    if (_accessToken.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] No access token. Attempting refresh first."));
        #endif
        int refreshHttpCode = performTokenRefresh();
        if(refreshHttpCode != 200) {
            // Si el refresco falla, no podemos continuar
            return refreshHttpCode;
        }
    }

    // 3. Preparar y enviar la petición de chequeo (/auth)
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiAuthPath;

    JsonDocument docAuthRequest;
    docAuthRequest["token"] = _accessToken; 
    String authRequestPayload;
    serializeJson(docAuthRequest, authRequestPayload);
    
    if (authRequestPayload.isEmpty() && !_accessToken.isEmpty()) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Failed to serialize auth request payload for /auth endpoint."));
        #endif
        return -106; // Error: Fallo serialización
    }

    int httpCode = _httpPost(fullUrl, _accessToken, authRequestPayload, responsePayload);

    // 4. Procesar respuesta
    if (httpCode == 200) {
        // Éxito: El token es válido. Parsea por si la respuesta incluye un
        // tiempo de colección actualizado.
        _parseAndStoreAuthResponse(responsePayload); 
    } else if (httpCode == 401) {
        // Error 401: El AccessToken es inválido. Intentar refrescar.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Auth endpoint returned 401. Attempting token refresh."));
        #endif
        httpCode = performTokenRefresh(); // Esta función maneja la desactivación si el refresco falla
    }
    return httpCode;
}

/**
 * @brief Intenta refrescar los tokens usando el RefreshToken.
 */
int API::performTokenRefresh() {
    // 1. No se puede refrescar si no hay RefreshToken
    if (_refreshToken.isEmpty()) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] No refresh token available. Cannot refresh. Deactivating."));
        #endif
        _updateAccessToken(""); // Limpiar tokens
        _updateActivationStatus(false); // Desactivar (estado crítico)
        return -105; // Error: No hay refresh token
    }

    // 2. Preparar payload de refresco
    JsonDocument doc;
    doc["token"] = _refreshToken;

    String payload;
    serializeJson(doc, payload);
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiRefreshTokenPath;

    // 3. Realizar POST
    int httpCode = _httpPost(fullUrl, "", payload, responsePayload); // Sin token de autorización

    // 4. Procesar respuesta
    if (httpCode == 200) {
        // Éxito: Parsear y guardar los *nuevos* tokens
        if (!_parseAndStoreAuthResponse(responsePayload)) {
            // Error: HTTP 200 pero la respuesta JSON está corrupta/incompleta
            httpCode = -103; // Error: Fallo de parseo
        }
    } else if (httpCode == 401) { 
        // Error 401: El RefreshToken es inválido o expiró.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] Refresh token rejected (401). Deactivating device."));
        #endif
        // Este es un estado crítico. El dispositivo debe ser reactivado.
        _updateAccessToken("");    
        _updateRefreshToken("");
        _updateActivationStatus(false); // Desactivar y guardar estado
    }
    return httpCode;
}

/**
 * @brief (Helper) Inicializa (carga/crea) la clave AES-256 desde NVS.
 */
bool API::_initAesKey() {
    nvs_handle_t nvsHandle;
    esp_err_t err;

    // 1. Abrir NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_Key] Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        #endif
        return false;
    }

    // 2. Intentar leer la clave
    size_t required_size = API_AES_KEY_SIZE;
    err = nvs_get_blob(nvsHandle, NVS_AES_KEY_NAME, _aesKey, &required_size);

    if (err == ESP_OK && required_size == API_AES_KEY_SIZE) {
        // Éxito: Clave cargada
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Key] AES key loaded successfully from NVS."));
        #endif
        nvs_close(nvsHandle);
        _aesKeyInitialized = true;
        return true;
        
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        // 3. No encontrada: Generar nueva clave
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Key] AES key not found in NVS. Generating a new one..."));
        #endif
        
        esp_fill_random(_aesKey, API_AES_KEY_SIZE);

        // 4. Guardar la nueva clave en NVS
        err = nvs_set_blob(nvsHandle, NVS_AES_KEY_NAME, _aesKey, API_AES_KEY_SIZE);
        if (err != ESP_OK) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_Key] Error (%s) saving new AES key to NVS!\n", esp_err_to_name(err));
            #endif
            nvs_close(nvsHandle);
            return false;
        }

        // 5. Hacer Commit
        err = nvs_commit(nvsHandle);
        if (err != ESP_OK) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_Key] Error (%s) committing new AES key to NVS!\n", esp_err_to_name(err));
            #endif
            nvs_close(nvsHandle);
            return false;
        }
        
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Key] New AES key generated and saved to NVS."));
        #endif
        nvs_close(nvsHandle);
        _aesKeyInitialized = true;
        return true;

    } else { 
        // Otro error de NVS
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_Key] Error (%s) reading AES key from NVS. Size: %d\n", esp_err_to_name(err), required_size);
        #endif
        nvs_close(nvsHandle);
        return false;
    }
}