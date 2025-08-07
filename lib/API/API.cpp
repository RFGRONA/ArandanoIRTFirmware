// lib/API/API.cpp
#include "API.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_random.h"
#include "mbedtls/aes.h" 
#include "mbedtls/gcm.h"
#include "mbedtls/base64.h"
#include "ErrorLogger.h"
#include "TimeManager.h"
#include "ConfigManager.h"
#include "EnvironmentDataJSON.h"
#include "MultipartDataSender.h"

#define HTTP_REQUEST_TIMEOUT 10000

#define AES_GCM_IV_LENGTH 12  
#define AES_GCM_TAG_LENGTH 16

// JSON keys for the API state file on SD
const char* JSON_KEY_ACCESS_TOKEN = "accessToken";
const char* JSON_KEY_REFRESH_TOKEN = "refreshToken";
const char* JSON_KEY_COLLECTION_TIME = "dataCollectionTime";
const char* JSON_KEY_IS_ACTIVATED = "isActivated";

// NVS definitions
#define NVS_NAMESPACE "api_secure"
#define NVS_AES_KEY_NAME "aes_key"

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
    if (!_initAesKey()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API CRITICAL] AES Key initialization FAILED. API state will not be encrypted/decrypted."));
        #endif
    }
    _loadPersistentData();
}

API::~API() {
}

void API::setDeviceMAC(const String& mac) {
    _deviceMACAddress = mac;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[API] Device MAC address set to: %s\n", _deviceMACAddress.c_str());
    #endif
}

bool API::_saveCurrentApiStateToSd() {
    if (!_sdManagerRef.isSDAvailable()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] SD card not available. Cannot save API state.");
        #endif
        return false;
    }

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

    if (_aesKeyInitialized) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] Encrypting API state...");
        #endif
        
        mbedtls_gcm_context gcm;
        unsigned char iv[AES_GCM_IV_LENGTH];
        unsigned char tag[AES_GCM_TAG_LENGTH];
        
        size_t plaintextLength = stateJsonToSavePlain.length();
        unsigned char* ciphertextBuffer = (unsigned char*)malloc(plaintextLength);
        if (!ciphertextBuffer) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Save] Failed to allocate memory for ciphertext buffer!");
            #endif
            return false;
        }

        esp_fill_random(iv, AES_GCM_IV_LENGTH);

        mbedtls_gcm_init(&gcm);
        int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _aesKey, API_AES_KEY_SIZE * 8);
        if (ret != 0) {
            mbedtls_gcm_free(&gcm); free(ciphertextBuffer); return false;
        }

        ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plaintextLength, 
                                        iv, AES_GCM_IV_LENGTH, NULL, 0, 
                                        (const unsigned char*)stateJsonToSavePlain.c_str(), ciphertextBuffer,
                                        AES_GCM_TAG_LENGTH, tag);
        mbedtls_gcm_free(&gcm);
        if (ret != 0) {
            free(ciphertextBuffer); return false;
        }

        // --- LÓGICA DE COMBINACIÓN CORREGIDA Y MÁS SEGURA ---
        size_t totalEncryptedLength = AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH + plaintextLength;
        std::vector<unsigned char> combinedBuffer(totalEncryptedLength);
        memcpy(combinedBuffer.data(), iv, AES_GCM_IV_LENGTH);
        memcpy(combinedBuffer.data() + AES_GCM_IV_LENGTH, tag, AES_GCM_TAG_LENGTH);
        memcpy(combinedBuffer.data() + AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH, ciphertextBuffer, plaintextLength);
        
        free(ciphertextBuffer);

        // --- LÓGICA DE BASE64 CORREGIDA ---
        size_t base64Len;
        mbedtls_base64_encode(NULL, 0, &base64Len, combinedBuffer.data(), combinedBuffer.size());
        std::vector<unsigned char> base64Buffer(base64Len);
        
        ret = mbedtls_base64_encode(base64Buffer.data(), base64Len, &base64Len, combinedBuffer.data(), combinedBuffer.size());
        if(ret != 0){ return false; }

        dataToStoreOnSd = String((char*)base64Buffer.data());
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] API state encrypted and Base64 encoded successfully.");
        #endif

    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Save] AES key not initialized. Saving API state as PLAINTEXT.");
        #endif
        dataToStoreOnSd = stateJsonToSavePlain;
    }

    bool success = _sdManagerRef.saveApiState(dataToStoreOnSd);
    return success;
}


void API::_loadPersistentData() {
    String stateJsonFromSdBase64; 
    if (_sdManagerRef.isSDAvailable() && _sdManagerRef.readApiState(stateJsonFromSdBase64) && !stateJsonFromSdBase64.isEmpty()) {
        
        String stateJsonPlain = ""; 

        if (_aesKeyInitialized && stateJsonFromSdBase64.length() > 50) { 
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Load] Data found on SD. Attempting decryption...");
            #endif
            
            size_t decodedLen;
            mbedtls_base64_decode(NULL, 0, &decodedLen, (const unsigned char*)stateJsonFromSdBase64.c_str(), stateJsonFromSdBase64.length());
            std::vector<unsigned char> encryptedBuffer(decodedLen);
            
            int ret = mbedtls_base64_decode(encryptedBuffer.data(), encryptedBuffer.size(), &decodedLen, (const unsigned char*)stateJsonFromSdBase64.c_str(), stateJsonFromSdBase64.length());

            if (ret != 0 || decodedLen < (AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH)) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Base64 decode failed (-0x%04X) or decoded data too short. Treating as corrupt.\n", -ret);
                #endif
                goto use_defaults;
            }
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_Load] Base64 decoded successfully. Total binary length: %d bytes.\n", decodedLen);
            #endif

            // --- LÓGICA DE EXTRACCIÓN CORREGIDA ---
            unsigned char iv[AES_GCM_IV_LENGTH];
            unsigned char tag[AES_GCM_TAG_LENGTH];
            size_t ciphertextLength = decodedLen - AES_GCM_IV_LENGTH - AES_GCM_TAG_LENGTH;
            
            memcpy(iv, encryptedBuffer.data(), AES_GCM_IV_LENGTH);
            memcpy(tag, encryptedBuffer.data() + AES_GCM_IV_LENGTH, AES_GCM_TAG_LENGTH);
            
            std::vector<unsigned char> plaintextBuffer(ciphertextLength);

            mbedtls_gcm_context gcm;
            mbedtls_gcm_init(&gcm);
            ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, _aesKey, API_AES_KEY_SIZE * 8);
            if (ret != 0) { mbedtls_gcm_free(&gcm); goto use_defaults; }

            ret = mbedtls_gcm_auth_decrypt(&gcm, ciphertextLength,
                                           iv, AES_GCM_IV_LENGTH, NULL, 0,
                                           tag, AES_GCM_TAG_LENGTH,
                                           encryptedBuffer.data() + AES_GCM_IV_LENGTH + AES_GCM_TAG_LENGTH, /* Puntero al ciphertext */
                                           plaintextBuffer.data());
            mbedtls_gcm_free(&gcm);

            if (ret == 0) {
                stateJsonPlain = String((char*)plaintextBuffer.data(), plaintextBuffer.size()); // Crear String con longitud explícita
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[API_Load] API state decrypted successfully!");
                #endif
            } else {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Decryption FAILED (mbedtls_gcm_auth_decrypt: -0x%04X). Data is corrupt or key is wrong.\n", -ret);
                #endif
                goto use_defaults;
            }

        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[API_Load] Loading API state as PLAINTEXT (key not ready or data seems unencrypted).");
            #endif
            stateJsonPlain = stateJsonFromSdBase64;
        }

        // --- Lógica de parseo (sin cambios) ---
        if (!stateJsonPlain.isEmpty()) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, stateJsonPlain);
            if (error) {
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[API_Load] Failed to parse API state JSON: %s. Using defaults.\n", error.c_str());
                #endif
                goto use_defaults;
            } else {
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
use_defaults:
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_Load] No valid API state found. Using defaults and saving a new state file.");
        #endif
        _accessToken = "";
        _refreshToken = "";
        _dataCollectionTimeMinutes = 0;
        _activatedFlag = false;
        if (_sdManagerRef.isSDAvailable()) {
             _saveCurrentApiStateToSd();
        }
    }

    if (_dataCollectionTimeMinutes <= 0) { _dataCollectionTimeMinutes = 0; }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[API_Load] Effective State after load: Activated: %s\n", _activatedFlag ? "Yes" : "No");
    #endif
}


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

// --- Public Getter Methods (no change) ---
bool API::isActivated() const { return _activatedFlag; }
String API::getAccessToken() const { return _accessToken; }
String API::getBaseApiUrl() const { return _apiBaseUrl; }
int API::getDataCollectionTimeMinutes() const {
    return _dataCollectionTimeMinutes > 0 ? _dataCollectionTimeMinutes : 0;
}

// --- Core API Interaction Methods ---
int API::_httpPost(const String& fullUrl, const String& authorizationToken, const String& jsonPayload, String& responsePayload) {
    HTTPClient http;
    int httpResponseCode = -1; 

    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[API_httpPost] No WiFi connection.");
        #endif
        return -100; 
    }
    
    http.setReuse(false); // Good practice for ESP32 HTTPClient
    if (http.begin(fullUrl)) {
        http.setTimeout(HTTP_REQUEST_TIMEOUT);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Connection", "close"); // Advised for ESP32 to ensure resources are freed
        if (!authorizationToken.isEmpty()) {
            http.addHeader("Authorization", "Device " + authorizationToken);
        }

        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_httpPost] POST to: %s\n", fullUrl.c_str());
            if(!jsonPayload.isEmpty()) Serial.printf("[API_httpPost] Payload: %s\n", jsonPayload.c_str());
        #endif

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
        httpResponseCode = -101; 
    }
    return httpResponseCode;
}

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
    if (newAccessToken && newRefreshToken) {
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
        // If essential tokens are missing, this is problematic.
        // Depending on API design, this might mean deactivation or error.
        // For now, we just note they are missing and don't update them.
        // If activation returned 200 but tokens are missing, activation logic should handle that.
    }
    
    // Only update collection time if a valid positive value is received
    if (newDataCollectionTime > 0) {
        _updateDataCollectionTime(newDataCollectionTime); // Will save combined state to SD
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_parse] DataCollectionTimeMinutes updated to: %d via _update method.\n", _dataCollectionTimeMinutes);
        #endif
    } else if (!doc[JSON_KEY_COLLECTION_TIME].isNull() && newDataCollectionTime <= 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_parse] Received invalid dataCollectionTimeMinutes: %d. Kept current: %d\n", newDataCollectionTime, _dataCollectionTimeMinutes);
        #endif
    }
    // If dataCollectionTimeMinutes is not in the response, _dataCollectionTimeMinutes member remains unchanged.

    return tokensProcessed; // Return true if at least tokens were successfully parsed and updated
}

int API::performActivation(const String& deviceId, const String& activationCode) {
    if (deviceId.isEmpty() || activationCode.isEmpty()) return -102;

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

    int httpCode = _httpPost(fullUrl, "", payload, responsePayload);

    if (httpCode == 200) {
        if (_parseAndStoreAuthResponse(responsePayload)) {
            _updateActivationStatus(true); // This will also save all current state to SD
        } else {
            httpCode = -103; // Parse failure or missing tokens
            _updateActivationStatus(false);
        }
    } else {
        _updateActivationStatus(false);
    }
    return httpCode;
}

int API::checkBackendAndAuth() {
    if (!_activatedFlag) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Device not activated. Skipping auth check."));
        #endif
        return -104; 
    }
    if (_accessToken.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] No access token. Attempting refresh first."));
        #endif
        int refreshHttpCode = performTokenRefresh();
        if(refreshHttpCode != 200) {
            // If refresh fails, and we didn't have an access token, we can't proceed.
            // performTokenRefresh itself handles deactivation on critical refresh failure.
            return refreshHttpCode;
        }
        // If refresh succeeded, _accessToken is now populated.
    }

    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiAuthPath;

    JsonDocument docAuthRequest;
    docAuthRequest["token"] = _accessToken; 
    String authRequestPayload;
    serializeJson(docAuthRequest, authRequestPayload);
    
    // If serialization failed for some reason but token was present
    if (authRequestPayload.isEmpty() && !_accessToken.isEmpty()) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Failed to serialize auth request payload for /auth endpoint."));
        #endif
        return -106; 
    }

    int httpCode = _httpPost(fullUrl, _accessToken, authRequestPayload, responsePayload);

    if (httpCode == 200) {
        // Parse and store any updated tokens/collection time.
        // The _activatedFlag itself shouldn't change on a successful /auth call.
        _parseAndStoreAuthResponse(responsePayload); 
    } else if (httpCode == 401) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_AuthCheck] Auth endpoint returned 401. Attempting token refresh."));
        #endif
        httpCode = performTokenRefresh(); // This will update state on SD if successful and handle deactivation on critical refresh failure
    }
    return httpCode;
}

int API::performTokenRefresh() {
    if (_refreshToken.isEmpty()) {
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] No refresh token available. Cannot refresh. Deactivating."));
        #endif
        _updateAccessToken(""); // Clear potentially invalid access token
        _updateActivationStatus(false); // Deactivate and save state to SD
        return -105; 
    }

    JsonDocument doc;
    doc["token"] = _refreshToken;

    String payload;
    serializeJson(doc, payload);
    String responsePayload;
    String fullUrl = _apiBaseUrl + _apiRefreshTokenPath;

    int httpCode = _httpPost(fullUrl, "", payload, responsePayload); // Refresh usually doesn't need prior auth with access token

    if (httpCode == 200) {
        if (!_parseAndStoreAuthResponse(responsePayload)) {
            // Successful HTTP but failed to parse or missing new tokens in response
            httpCode = -103; // Custom error for parse failure
            // Don't deactivate here yet, let next checkBackendAndAuth or operation fail if tokens are truly bad.
            // Or, if API guarantees new tokens on 200, this implies a server/contract issue.
        }
        // Activation status should remain true if refresh is successful.
        // _parseAndStoreAuthResponse would update tokens, which then get saved via _saveCurrentApiStateToSd
    } else if (httpCode == 401) { // Refresh token itself is invalid/expired
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Refresh] Refresh token rejected (401). Deactivating device."));
        #endif
        _updateAccessToken("");    // Clear tokens
        _updateRefreshToken("");
        _updateActivationStatus(false); // Deactivate and save this state to SD
    }
    return httpCode;
}

bool API::_initAesKey() {
    nvs_handle_t nvsHandle;
    esp_err_t err;

    // Open NVS
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_Key] Error (%s) opening NVS handle!\n", esp_err_to_name(err));
        #endif
        return false;
    }

    // Try to read the key
    size_t required_size = API_AES_KEY_SIZE;
    err = nvs_get_blob(nvsHandle, NVS_AES_KEY_NAME, _aesKey, &required_size);

    if (err == ESP_OK && required_size == API_AES_KEY_SIZE) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Key] AES key loaded successfully from NVS."));
        #endif
        nvs_close(nvsHandle);
        _aesKeyInitialized = true;
        return true;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[API_Key] AES key not found in NVS. Generating a new one..."));
        #endif
        
        // Generate a new random key
        esp_fill_random(_aesKey, API_AES_KEY_SIZE);

        // Save the new key to NVS
        err = nvs_set_blob(nvsHandle, NVS_AES_KEY_NAME, _aesKey, API_AES_KEY_SIZE);
        if (err != ESP_OK) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[API_Key] Error (%s) saving new AES key to NVS!\n", esp_err_to_name(err));
            #endif
            nvs_close(nvsHandle);
            return false;
        }

        // Commit changes
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

    } else { // Other error reading BLOB
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[API_Key] Error (%s) reading AES key from NVS. Size: %d\n", esp_err_to_name(err), required_size);
        #endif
        nvs_close(nvsHandle);
        return false;
    }
}