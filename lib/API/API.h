#ifndef API_H
#define API_H

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "LEDStatus.h"

// Endpoints del backend (se usa HTTPS)
#define ACTIVATION_ENDPOINT       "https://backend.example.com/activate"
#define CHECK_ACTIVATION_ENDPOINT "https://backend.example.com/check_activation/"
#define BACKEND_STATUS_ENDPOINT   "https://backend.example.com/status"

// JWT de activación y ID del dispositivo (definidos directamente en el código)
#define ACTIVATION_JWT "tu_jwt_de_activacion_aqui"
#define DEVICE_ID      "tu_device_id_aqui"

// Estados de activación que se almacenarán en memoria no volátil
#define STATE_ACTIVATED    "ACTIVATED"
#define STATE_NO_ACTIVATED "NO_ACTIVATED"

// Claves para guardar datos en Preferences
#define PREF_ACTIVATION_STATE "activation_state"
#define PREF_AUTH_TOKEN       "auth_token"

// Configuración para peticiones HTTP
#define HTTP_TIMEOUT     10000   // Timeout en milisegundos (10 segundos)
#define HTTP_MAX_RETRIES 3       // Número máximo de reintentos

class API {
public:
    API();

    /**
     * Envía el código de activación (JWT) al endpoint de activación.
     * Si recibe 200, guarda el estado "ACTIVATED" en la memoria no volátil.
     * En caso contrario, muestra ERROR_AUTH en el LED y guarda "NO_ACTIVATED".
     * @return true si se activó correctamente, false en caso de error.
     */
    bool activateDevice();

    /**
     * Verifica que el dispositivo siga activo enviando su ID al endpoint.
     * Si recibe 200, se espera un JSON con un token de autenticación:
     *   - Si el token recibido es diferente al actual, se guarda en memoria volátil y no volátil.
     *   - Si es el mismo, no se actualiza la memoria.
     * Si no se recibe un 200, se guarda "NO_ACTIVATED" y se muestra ERROR_AUTH en el LED.
     * @return true si el dispositivo sigue activo, false en caso contrario.
     */
    bool checkActivation();

    /**
     * Realiza una solicitud al endpoint para verificar que el backend está online.
     * Se considera online si se recibe un código 200 y un mensaje JSON con "status":"OK".
     * @return true si el backend está online, false en caso contrario.
     */
    bool checkBackendStatus();

    /**
     * Devuelve el token de autenticación actual almacenado.
     * @return String con el token.
     */
    String getAuthToken();

private:
    LEDStatus _led;             // Para indicar errores y estados mediante LED
    Preferences _preferences;   // Para guardar el estado de activación y token en memoria no volátil
    String _authToken;          // Token de autenticación (memoria volátil)

    // Guarda el estado de activación ("ACTIVATED" o "NO_ACTIVATED") en memoria no volátil
    void saveActivationState(const char* state);

    // Guarda el token de autenticación en memoria no volátil
    void saveAuthToken(const String &token);
};

#endif // API_H