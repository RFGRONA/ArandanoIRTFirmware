/**
 * @file WiFiManager.h
 * @brief Define una clase para gestionar el estado de conexión WiFi (STA)
 * y manejar la reconexión no bloqueante.
 *
 * Encapsula la lógica de conexión WiFi, incluyendo:
 * - Almacenamiento de credenciales (SSID, contraseña).
 * - Inicio de intentos de conexión.
 * - Seguimiento del estado mediante una máquina de estados (`ConnectionStatus`).
 * - Manejo de eventos estándar de WiFi (conexión, desconexión, IP obtenida)
 * mediante callbacks estáticos.
 * - Implementación de lógica de reconexión temporizada y no bloqueante.
 *
 * Utiliza un patrón singleton (`_instance`) para que los manejadores de
 * eventos estáticos (requeridos por el sistema de eventos de WiFi)
 * puedan actualizar el estado de la instancia activa.
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>          // Librería WiFi de ESP32
#include <HTTPClient.h>    // Requerido para el chequeo de conexión a Internet
#include "LEDStatus.h"     // Requerido para la retroalimentación visual (LED)

// --- Parámetros de Temporización y Reintentos de WiFi ---
///< Tiempo máx. (ms) a esperar en estado 'CONNECTING' antes de asumir timeout.
#define WIFI_CONNECT_TIMEOUT        20000  
///< Tiempo (ms) a esperar entre reintentos automáticos tras una desconexión.
#define WIFI_RECONNECT_INTERVAL     5000   
///< Núm. máx. de reintentos automáticos antes de pasar al estado 'CONNECTION_FAILED'.
#define MAX_WIFI_RECONNECT_ATTEMPTS 5      
// -----------------------------------------------------

/**
 * @class WiFiManager
 * @brief Gestiona el ciclo de vida, estado y reconexión automática del WiFi.
 */
class WiFiManager {
public:
    /**
     * @brief Define los posibles estados de la conexión WiFi.
     */
    enum ConnectionStatus {
        DISCONNECTED,      ///< Estado: Desconectado, sin intento de conexión en curso.
        CONNECTING,        ///< Estado: Un intento de conexión está activamente en progreso.
        CONNECTED,         ///< Estado: Conectado exitosamente al AP y con una IP asignada.
        CONNECTION_FAILED, ///< Estado: Se alcanzó el máx. de reintentos. Requiere intervención manual (o reinicio).
        CONNECTION_LOST    ///< Estado: Estaba conectado, pero se perdió la conexión. Intentará reconectar.
    };

    /**
     * @brief Constructor.
     * Inicializa el estado interno y registra los callbacks de eventos WiFi.
     */
    WiFiManager();

    /**
     * @brief Realiza la inicialización básica de WiFi.
     * Pone el ESP32 en modo Estación (STA) y deshabilita la
     * autoreconexión nativa de ESP-IDF (`WiFi.setAutoReconnect(false)`),
     * ya que esta clase implementa su propia lógica en `handleWiFi()`.
     * @return `true` (asume éxito en la configuración básica).
     */
    bool begin();

    /**
     * @brief Inicia un intento de conexión a la red WiFi configurada.
     * Si el estado es `DISCONNECTED` o `CONNECTION_LOST`, cambia el estado a
     * `CONNECTING`, resetea contadores y llama a `WiFi.begin()`.
     * @return `true` si se inició un nuevo intento o si ya estaba conectado.
     * `false` si ya estaba en estado `CONNECTING`.
     */
    bool connectToWiFi();

    /**
     * @brief Obtiene el estado actual de la conexión WiFi.
     * @return El estado actual (enum `ConnectionStatus`).
     */
    ConnectionStatus getConnectionStatus();

    /**
     * @brief Fuerza una desconexión inmediata de la red WiFi.
     * Llama a `WiFi.disconnect()` y establece el estado interno a `DISCONNECTED`.
     */
    void disconnect();

    /**
     * @brief Maneja la máquina de estados de WiFi (reconexión y timeouts).
     * **Debe ser llamado repetidamente** en el `loop()` principal.
     * Verifica si se necesita una reconexión (basado en el estado y temporizadores)
     * y maneja los timeouts del estado `CONNECTING`.
     */
    void handleWiFi();

    /**
     * @brief Realiza un chequeo básico de conectividad a Internet (HTTP GET).
     * Verifica no solo la conexión WiFi, sino también la resolución DNS y
     * el enrutamiento básico.
     * @param testUrl URL para el chequeo (por defecto, "generate_204" de Google).
     * @return `true` solo si el estado es `CONNECTED` *y* la petición HTTP
     * retorna un código de éxito (200 OK o 204 No Content).
     * `false` en cualquier otro caso.
     */
    bool checkInternetConnection(const String& testUrl = "http://clients3.google.com/generate_204");

    /**
     * @brief Establece las credenciales (SSID y contraseña) a usar.
     * @param ssid Nombre de la red WiFi (SSID).
     * @param password Contraseña de la red WiFi.
     */
    void setCredentials(const String& ssid, const String& password);

     /**
     * @brief Obtiene la dirección MAC de la interfaz WiFi (STA) del ESP32.
     * @return La dirección MAC como String (ej. "AA:BB:CC:DD:EE:FF").
     */
    String getMacAddress() const; 

private:
    ///< Instancia de LEDStatus para retroalimentación visual del estado.
    LEDStatus _led;                     
    ///< Timestamp (millis) del último intento de reconexión o desconexión.
    unsigned long _lastReconnectAttempt = 0; 
    ///< Contador de intentos de reconexión automáticos consecutivos.
    unsigned int _reconnectAttempts = 0;  
    ///< Estado actual de la conexión (según `ConnectionStatus`).
    ConnectionStatus _currentStatus = DISCONNECTED; 
    ///< Puntero estático a la instancia única (patrón Singleton) para los callbacks.
    static WiFiManager* _instance;      

    // Credenciales almacenadas
    String _ssid;                       ///< Almacena el SSID objetivo.
    String _password;                   ///< Almacena la contraseña objetivo.

    // --- Manejadores de Eventos Estáticos ---
    // (Registrados en el sistema de eventos de ESP-IDF. Usan `_instance`
    // para afectar al objeto principal).

    /**
     * @brief Callback estático para `ARDUINO_EVENT_WIFI_STA_CONNECTED`.
     * Indica asociación con el AP (pero aún sin IP).
     */
    static void onWiFiConnected(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Callback estático para `ARDUINO_EVENT_WIFI_STA_GOT_IP`.
     * Indicador principal de conexión exitosa. Actualiza el estado a `CONNECTED`.
     */
    static void onWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info);

    /**
     * @brief Callback estático para `ARDUINO_EVENT_WIFI_STA_DISCONNECTED`.
     * Indica pérdida de conexión. Actualiza el estado a `CONNECTION_LOST`.
     */
    static void onWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    // --- Fin Handlers Estáticos ---

    /**
     * @brief (Helper) Intenta realizar una reconexión.
     * Verifica `_reconnectAttempts`. Si es < MAX, incrementa el contador,
     * establece el estado a `CONNECTING` y llama a `WiFi.begin()`.
     * Si se alcanza el MAX, establece el estado a `CONNECTION_FAILED`.
     */
    void attemptReconnect();
};

#endif // WIFI_MANAGER_H