#include "API.h"

API::API() {
    // Inicia el Preferences en el namespace "activation"
    _preferences.begin("activation", false);
    // Cargar token previamente guardado (si existe)
    _authToken = _preferences.getString(PREF_AUTH_TOKEN, "");
}

void API::saveActivationState(const char* state) {
    _preferences.putString(PREF_ACTIVATION_STATE, state);
}

void API::saveAuthToken(const String &token) {
    _preferences.putString(PREF_AUTH_TOKEN, token);
    _authToken = token;
}

bool API::activateDevice() {
    int attempt = 0;
    int httpResponseCode = 0;
    bool activated = false;
    
    // Intentar hasta HTTP_MAX_RETRIES
    while (attempt < HTTP_MAX_RETRIES && !activated) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);
        http.begin(ACTIVATION_ENDPOINT);
        http.addHeader("Content-Type", "application/json");

        // Construir payload JSON con el JWT de activación
        JsonDocument doc;
        doc["activation_code"] = ACTIVATION_JWT;
        String payload;
        serializeJson(doc, payload);

        httpResponseCode = http.POST(payload);
        http.end();

        if (httpResponseCode == 200) {
            // Activación correcta: guardar estado ACTIVATED
            saveActivationState(STATE_ACTIVATED);
            activated = true;
        }
        else {
            // Incrementa el contador de reintentos
            attempt++;
            delay(500); // breve retardo entre reintentos
        }
    }

    // Si no se activó, se indica error en el LED y se guarda NO_ACTIVATED
    if (!activated) {
        _led.setState(ERROR_AUTH);
        delay(5000); // Breve retardo para mostrar el error
        _led.setState(OFF); // Apagar el LED
        saveActivationState(STATE_NO_ACTIVATED);
    }
    
    return activated;
}

bool API::checkActivation() {
    int attempt = 0;
    int httpResponseCode = 0;
    bool active = false;
    
    // Se enviará la solicitud usando GET, incluyendo el device id en la URL
    // y el JWT de autenticación en la cabecera "Authorization"
    while (attempt < HTTP_MAX_RETRIES && !active) {
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT);
        
        // Construir la URL agregando el device id como parámetro
        String url = String(CHECK_ACTIVATION_ENDPOINT) + DEVICE_ID;
        http.begin(url);
        http.addHeader("Authorization", ACTIVATION_JWT);
        
        // Realizar solicitud GET
        httpResponseCode = http.GET();
        if (httpResponseCode == 200) {
            String response = http.getString();
            http.end();
            
            // Se espera que la respuesta contenga un JSON con el token
            JsonDocument responseDoc;
            DeserializationError error = deserializeJson(responseDoc, response);
            if (!error) {
                const char* newToken = responseDoc["token"];
                if (newToken != nullptr) {
                    String receivedToken(newToken);
                    // Actualiza el token si es diferente al actual
                    if (receivedToken != _authToken) {
                        saveAuthToken(receivedToken);
                    }
                    active = true;
                    break;
                }
            }
        } else {
            http.end();
        }
        attempt++;
        delay(500); // breve retardo entre reintentos
    }
    
    if (!active) {
        _led.setState(ERROR_AUTH);
        delay(5000); // Breve retardo para mostrar el error
        saveActivationState(STATE_NO_ACTIVATED);
    }
    
    return active;
}

bool API::checkBackendStatus() {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT);
    http.begin(BACKEND_STATUS_ENDPOINT);
    
    // Simplemente se evalúa que el backend responda con código 200
    int httpResponseCode = http.GET();
    http.end();

    if (httpResponseCode != 200) {
        _led.setState(ERROR_SEND);
        delay(5000);
        _led.setState(OFF); // Apagar el LED
        return false; // Backend no disponible
    }
    
    return true;
}

String API::getAuthToken() {
    return _authToken;
}
