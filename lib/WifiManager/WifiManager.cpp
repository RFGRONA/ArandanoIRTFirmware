#include "WiFiManager.h"
#include <HTTPClient.h> // Asegúrate que esta inclusión esté en WiFiManager.h si no está ya

WiFiManager* WiFiManager::_instance = nullptr;

WiFiManager::WiFiManager(const char* ssid, const char* password) {
    _ssid = String(ssid);
    _password = String(password);
    _reconnectAttempts = 0;
    _currentStatus = DISCONNECTED;

    if (_instance == nullptr) {
        _instance = this;
    } // else: Advertencia de múltiples instancias eliminada

    // Desregistrar eventos previos
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.removeEvent(ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Registrar los nuevos eventos
    WiFi.onEvent(onWiFiConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(onWiFiGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(onWiFiDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
}

bool WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false);
    delay(100);
    return true;
}

bool WiFiManager::connectToWiFi() {
    if (_currentStatus == CONNECTED || _currentStatus == CONNECTING) {
         return _currentStatus == CONNECTED;
    }

    _currentStatus = CONNECTING;
    _led.setState(CONNECTING_WIFI); // Asume que CONNECTING_WIFI está definido
    _reconnectAttempts = 0;
    _lastReconnectAttempt = millis();

    WiFi.disconnect(false);
    delay(100);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    return true; // Indica que se inició el intento
}

void WiFiManager::disconnect() {
    WiFi.disconnect(false);
    _currentStatus = DISCONNECTED;
    _led.setState(ERROR_WIFI); // Asume que ERROR_WIFI está definido
}

void WiFiManager::attemptReconnect() {
    if (_currentStatus == CONNECTING || _currentStatus == CONNECTED) return;

    if (millis() - _lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
        if (_reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
            _reconnectAttempts++;
            _currentStatus = CONNECTING;
            _led.setState(CONNECTING_WIFI);
            _lastReconnectAttempt = millis();

            WiFi.disconnect(false);
            delay(100);
            WiFi.begin(_ssid.c_str(), _password.c_str());
        } else {
             _currentStatus = CONNECTION_FAILED;
             _led.setState(ERROR_WIFI);
        }
    }
}

void WiFiManager::handleWiFi() {
    if (_currentStatus == DISCONNECTED || _currentStatus == CONNECTION_LOST || _currentStatus == CONNECTION_FAILED) {
       if(_currentStatus != CONNECTION_FAILED || _reconnectAttempts < MAX_WIFI_RECONNECT_ATTEMPTS) {
           attemptReconnect();
       }
    }

    if (_currentStatus == CONNECTING && (millis() - _lastReconnectAttempt > WIFI_CONNECT_TIMEOUT)) {
        WiFi.disconnect(false);
        _currentStatus = (_reconnectAttempts >= MAX_WIFI_RECONNECT_ATTEMPTS) ? CONNECTION_FAILED : CONNECTION_LOST;
        _led.setState(ERROR_WIFI);
        _lastReconnectAttempt = millis();
    }
}

WiFiManager::ConnectionStatus WiFiManager::getConnectionStatus() {
    return _currentStatus;
}

// --- Callbacks Estáticos ---

void WiFiManager::onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    // No hacer nada aquí, esperar a obtener IP
}

void WiFiManager::onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance != nullptr) {
        _instance->_currentStatus = CONNECTED;
        _instance->_reconnectAttempts = 0;
        _instance->_led.setState(ALL_OK); // Asume ALL_OK definido
    }
}

void WiFiManager::onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (_instance != nullptr) {
        _instance->_currentStatus = CONNECTION_LOST;
        _instance->_led.setState(ERROR_WIFI);
        _instance->_lastReconnectAttempt = millis();
    } // else: Mensaje de instancia nula eliminado
}

bool WiFiManager::checkInternetConnection(const String& testUrl) {
    if (_currentStatus != CONNECTED || WiFi.status() != WL_CONNECTED) {
        return false;
    }

    HTTPClient http;
    // Considera hacer este timeout configurable o más corto si es crítico
    http.setConnectTimeout(5000);
    http.begin(testUrl);
    int httpCode = http.GET();
    http.end();

    return (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_NO_CONTENT);
}