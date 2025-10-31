/**
 * @file WiFiManager.cpp
 * @brief Implementa la clase WiFiManager para la gestión de conexión WiFi.
 */
#include "WiFiManager.h"
// (El .h ya incluye WiFi.h, HTTPClient.h, y LEDStatus.h)
#include <WiFiClientSecure.h> // Incluido por el usuario, se mantiene aunque no se use activamente.

// Timeout para el chequeo de conectividad a Internet
#define INTERNET_CHECK_TIMEOUT 5000 

// Inicializa el puntero estático (Singleton)
WiFiManager* WiFiManager::_instance = nullptr;

/**
 * @brief Constructor.
 */
WiFiManager::WiFiManager() {
    _instance = this; // Asigna la instancia estática a este objeto
    _led.begin(); // Inicializa el LED
    
    // Registra las funciones estáticas como callbacks para los eventos de WiFi
    WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

/**
 * @brief Inicialización básica del modo STA.
 */
bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA); // Establece el modo Estación
    WiFi.setAutoReconnect(false); // Deshabilita la autoreconexión de ESP-IDF
    return true;
}

/**
 * @brief Almacena las credenciales.
 */
void WiFiManager::setCredentials(const String& ssid, const String& password) {
    _ssid = ssid;
    _password = password;
}

/**
 * @brief Inicia un intento de conexión (si no está ya conectado o conectando).
 */
bool WiFiManager::connectToWiFi() {
    if (_ssid.isEmpty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.println(F("[WiFiManager] Error: No SSID configured."));
        #endif
        _led.setState(ERROR_WIFI); // Indica error de configuración
        return false;
    }

    switch (_currentStatus) {
        case CONNECTED:
            #ifdef ENABLE_DEBUG_SERIAL
                // Salida de terminal en inglés
                Serial.println(F("[WiFiManager] connectToWiFi() called, but already connected."));
            #endif
            return true; // Ya conectado
        case CONNECTING:
            #ifdef ENABLE_DEBUG_SERIAL
                // Salida de terminal en inglés
                Serial.println(F("[WiFiManager] connectToWiFi() called, but already connecting."));
            #endif
            return false; // Intento ya en progreso
        case DISCONNECTED:
        case CONNECTION_LOST:
        case CONNECTION_FAILED: // Permite reintentar desde el estado FAILED
            #ifdef ENABLE_DEBUG_SERIAL
                // Salida de terminal en inglés
                Serial.println(F("[WiFiManager] connectToWiFi() initiating new connection."));
            #endif
            _reconnectAttempts = 0; // Resetea el contador para un intento manual/inicial
            attemptReconnect();
            return true;
        default:
             return false; // No debería ocurrir
    }
}

/**
 * @brief (Helper Interno) Realiza el intento de reconexión.
 */
void WiFiManager::attemptReconnect() {
    // Verifica si se ha superado el límite de reintentos
    if (_reconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.printf("[WiFiManager] Max (%d) reconnect attempts reached. Setting to CONNECTION_FAILED.\n", MAX_WIFI_RECONNECT_ATTEMPTS);
        #endif
        _currentStatus = CONNECTION_FAILED;
        _led.setState(ERROR_WIFI); // LED de error (fijo)
        return;
    }

    _reconnectAttempts++;
    _currentStatus = CONNECTING;
    _led.setState(CONNECTING_WIFI); // LED en modo "conectando"
    _lastReconnectAttempt = millis(); // Inicia el temporizador de timeout
    
    #ifdef ENABLE_DEBUG_SERIAL
        // Salida de terminal en inglés
        Serial.printf("[WiFiManager] Attempting WiFi connection (Attempt %d/%d)...\n", _reconnectAttempts, MAX_WIFI_RECONNECT_ATTEMPTS);
    #endif
    
    // Inicia la conexión WiFi
    WiFi.begin(_ssid.c_str(), _password.c_str());
}

/**
 * @brief Máquina de estados principal (debe llamarse en loop()).
 */
void WiFiManager::handleWiFi() {
    unsigned long now = millis();

    switch (_currentStatus) {
        case CONNECTING:
            // Manejo de Timeout: Verifica si está tardando demasiado en conectar
            if (now - _lastReconnectAttempt > WIFI_CONNECT_TIMEOUT) {
                #ifdef ENABLE_DEBUG_SERIAL
                    // Salida de terminal en inglés
                    Serial.printf("[WiFiManager] Connection timeout (%dms) reached. Disconnecting to retry...\n", WIFI_CONNECT_TIMEOUT);
                #endif
                WiFi.disconnect(true); // Fuerza la desconexión
                _currentStatus = CONNECTION_LOST; // Trata el timeout como una desconexión
                _lastReconnectAttempt = now; // Reinicia el timer para el *intervalo* de reintento
                _led.setState(ERROR_WIFI);
            }
            break;

        case CONNECTION_LOST:
        case DISCONNECTED: // Trata DISCONNECTED igual que LOST (reintento automático)
            // Manejo de Reintento: Verifica si ya pasó el intervalo de espera
            if (now - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
                #ifdef ENABLE_DEBUG_SERIAL
                    // Salida de terminal en inglés
                    if(_currentStatus == CONNECTION_LOST) Serial.println(F("[WiFiManager] Connection lost. Retrying..."));
                    else Serial.println(F("[WiFiManager] Disconnected. Retrying..."));
                #endif
                attemptReconnect();
            }
            break;

        case CONNECTION_FAILED:
            // En este estado, se detienen los reintentos automáticos.
            // Se requiere una llamada manual a connectToWiFi() para reintentar.
            break;

        case CONNECTED:
            // Todo bien, no hacer nada.
            break;
    }
}

/**
 * @brief Fuerza la desconexión.
 */
void WiFiManager::disconnect() {
    #ifdef ENABLE_DEBUG_SERIAL
        // Salida de terminal en inglés
        Serial.println(F("[WiFiManager] Forcing disconnection..."));
    #endif
    WiFi.disconnect(true);
    _currentStatus = DISCONNECTED;
    _reconnectAttempts = 0;
    _led.setState(ERROR_WIFI); // Indica desconexión manual/error
}

/**
 * @brief Retorna el estado actual.
 */
WiFiManager::ConnectionStatus WiFiManager::getConnectionStatus() {
    return _currentStatus;
}

/**
 * @brief Retorna la dirección MAC.
 */
String WiFiManager::getMacAddress() const {
    return WiFi.macAddress();
}

/**
 * @brief Chequea la conectividad a Internet.
 */
bool WiFiManager::checkInternetConnection(const String& testUrl) {
    if (_currentStatus != CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.println(F("[WiFiManager] Internet check failed: WiFi is not in CONNECTED state."));
        #endif
        return false;
    }

    HTTPClient http;
    int httpCode = 0;
    
    #ifdef ENABLE_DEBUG_SERIAL
        // Salida de terminal en inglés
        Serial.println("[WiFiManager] Performing connectivity check to: " + testUrl);
    #endif

    http.setReuse(false); // No reutilizar conexión para un chequeo simple
    
    // Se usa http.begin(url) estándar (solo HTTP)
    if (http.begin(testUrl)) { 
        http.setTimeout(INTERNET_CHECK_TIMEOUT);
        
        httpCode = http.GET();
        
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.printf("[WiFiManager] Internet check got HTTP code: %d\n", httpCode);
        #endif

        http.end();
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.println(F("[WiFiManager] http.begin() failed for Internet check."));
        #endif
        return false;
    }

    // Éxito = 200 (OK) o 204 (No Content)
    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT);
}


// --- Implementación de Handlers de Eventos Estáticos ---

/**
 * @brief Evento: Asociado al AP (esperando IP).
 */
void WiFiManager::onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.println(F("[WiFiManager] Event: WiFi STA Connected (Associated with AP). Waiting for IP..."));
        #endif
        // No cambiamos el estado aquí, esperamos a GOT_IP
    }
}

/**
 * @brief Evento: IP Obtenida (Conexión completa).
 */
void WiFiManager::onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.print(F("[WiFiManager] Event: WiFi STA Got IP: "));
            Serial.println(WiFi.localIP());
        #endif
        _instance->_currentStatus = CONNECTED;
        _instance->_reconnectAttempts = 0; // Resetea el contador al conectar
        _instance->_led.setState(ALL_OK); // LED en estado OK
    }
}

/**
 * @brief Evento: Desconectado del AP.
 */
void WiFiManager::onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Salida de terminal en inglés
            Serial.printf("[WiFiManager] Event: WiFi STA Disconnected. Reason: %d\n", info.wifi_sta_disconnected.reason);
        #endif
        
        // Si estábamos conectados o conectando, pasamos a CONNECTION_LOST
        if (_instance->_currentStatus == CONNECTED || _instance->_currentStatus == CONNECTING) {
            _instance->_currentStatus = CONNECTION_LOST;
        }
        // Si ya estábamos en DISCONNECTED (manual) o FAILED, se mantiene.
        
        _instance->_lastReconnectAttempt = millis(); // Inicia el timer para el intervalo de reintento
        _instance->_led.setState(ERROR_WIFI); // LED en estado de error/desconexión
    }
}