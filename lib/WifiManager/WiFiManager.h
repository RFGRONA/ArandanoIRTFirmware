#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <HTTPClient.h>
#include "LEDStatus.h" // Asegúrate que este archivo exista y funcione

#define WIFI_CONNECT_TIMEOUT        20000  // 20 segundos de timeout de conexión
#define WIFI_RECONNECT_INTERVAL     5000   // 5 segundos entre intentos de reconexión (reducido para pruebas)
#define MAX_WIFI_RECONNECT_ATTEMPTS 5      // Máximo de intentos de reconexión (aumentado un poco)

class WiFiManager {
public:
    enum ConnectionStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        CONNECTION_FAILED,
        CONNECTION_LOST // Nuevo estado para cuando se pierde la conexión
    };

    // Constructor: recibe SSID y contraseña como parámetros
    WiFiManager(const char* ssid, const char* password);

    // Inicializa la configuración WiFi (modo STA)
    bool begin();

    // Intenta conectar a la red WiFi usando las credenciales proporcionadas
    bool connectToWiFi();

    // Devuelve el estado de conexión actual
    ConnectionStatus getConnectionStatus();

    // Fuerza la desconexión de la red WiFi
    void disconnect();

    // Gestiona el estado y la reconexión (llamar desde loop)
    void handleWiFi();

    // Verifica la conectividad a internet haciendo una petición HTTP GET
    bool checkInternetConnection(const String& testUrl = "http://clients3.google.com/generate_204");

private:
    LEDStatus _led;
    unsigned long _lastReconnectAttempt = 0; // Para reconexión no bloqueante
    unsigned int _reconnectAttempts = 0;     // Cambiado a unsigned int
    ConnectionStatus _currentStatus = DISCONNECTED;
    static WiFiManager* _instance;

    // --- CAMBIO PRINCIPAL ---
    // Almacenar copias de las credenciales usando String
    String _ssid;
    String _password;
    // --- FIN CAMBIO ---

    // Función que se ejecuta cuando se detecta la desconexión de la red WiFi
    static void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info); // Declaración (si la usas)

    // Renombrado para claridad
    static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);
    static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);
    static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);

    // Método privado para intentar reconectar (ahora llamado desde handleWiFi)
    void attemptReconnect();
};

#endif