#include "TimeManager.h"
#include <WiFi.h> // Para verificar el estado de WiFi (WiFi.status())

// Timeouts para la función 'getLocalTime' (bloqueante)
#define NTP_PER_SERVER_TIMEOUT_MS 15000 // 15 segundos por servidor
#define NTP_FINAL_WAIT_MS 5000 // 5 segundos después del último timeout

TimeManager::TimeManager() : 
    _timeSynchronized(false),
    _ntpServer1(DEFAULT_NTP_SERVER_1),
    _ntpServer2(DEFAULT_NTP_SERVER_2),
    _gmtOffset_sec(0),
    _daylightOffset_sec(0) {
    // Constructor (usa lista de inicialización)
}

void TimeManager::begin(const String& ntpServer1, const String& ntpServer2, long gmtOffset_sec, int daylightOffset_sec) {
    // Guarda la configuración
    _ntpServer1 = ntpServer1;
    _ntpServer2 = ntpServer2;
    _gmtOffset_sec = gmtOffset_sec;
    _daylightOffset_sec = daylightOffset_sec;

    // Llama a la función de ESP-IDF 'configTime' que configura e inicia
    // el servicio SNTP (Simple Network Time Protocol) en segundo plano.
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1.c_str(), _ntpServer2.c_str());
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[TimeManager] Initialized with NTP Servers: %s, %s. GMT Offset: %ld, DST Offset: %d\n",
                      _ntpServer1.c_str(), _ntpServer2.c_str(), _gmtOffset_sec, _daylightOffset_sec);
    #endif
}

bool TimeManager::syncNtpTime() {
    // Pre-condición: Se necesita WiFi para sincronizar.
    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[TimeManager] NTP Sync failed: WiFi not connected.");
        #endif
        _timeSynchronized = false; // Marca como no sincronizado si se cae el WiFi
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[TimeManager] Attempting NTP time synchronization");
    #endif

    // 'getLocalTime' es una función bloqueante de ESP-IDF.
    // Esperará hasta obtener la hora o hasta que se cumpla el timeout interno.
    
    struct tm timeinfo;
    int retries = 0;
    
    // Intentamos obtener la hora. Si falla, reintentamos (hasta NTP_SYNC_MAX_RETRIES).
    // El timeout total es una aproximación de los reintentos * timeouts internos.
    while(!getLocalTime(&timeinfo, (NTP_SYNC_MAX_RETRIES * NTP_PER_SERVER_TIMEOUT_MS) + NTP_FINAL_WAIT_MS)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(NTP_SYNC_RETRY_DELAY_MS);
        retries++;
        if (retries >= NTP_SYNC_MAX_RETRIES) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("\n[TimeManager] Failed to obtain NTP time after multiple retries.");
            #endif
            _timeSynchronized = false; 
            return false;
        }
        
        // Aborta si se pierde el WiFi durante el intento
        if (WiFi.status() != WL_CONNECTED) {
            #ifdef ENABLE_DEBUG_SERIAL
                 Serial.println("\n[TimeManager] NTP Sync aborted: WiFi disconnected during sync attempt.");
            #endif
            _timeSynchronized = false;
            return false;
        }
    }
    
    // Sincronización exitosa
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("\n[TimeManager] NTP Time synchronized successfully.");
        Serial.printf("[TimeManager] Current time: %s", asctime(&timeinfo)); // asctime añade \n
    #endif
    _timeSynchronized = true;
    return true;
}

String TimeManager::getCurrentTimestampString(bool forFileNames) {
    // Fallback: Si no hay NTP, retorna un timestamp basado en millis() (tiempo de actividad).
    if (!_timeSynchronized) {
        unsigned long ms = millis();
        unsigned long s = ms / 1000;
        unsigned long m = s / 60;
        unsigned long h = m / 60;
        // Formato: UPTIME_HHhMMmSSs
        char buf[30];
        sprintf(buf, "UPTIME_%02luh%02lum%02lus", h % 24, m % 60, s % 60);
        return String(buf);
    }

    // Obtiene la estructura 'tm' (debería ser instantáneo si ya está sincronizado).
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[TimeManager] Failed to get local time structure even after sync.");
        #endif
        return "TIME_STRUCT_ERROR";
    }

    char buf[32];
    if (forFileNames) {
        // Formato para nombres de archivo: YYYYMMDD_HHMMSS
        strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &timeinfo);
    } else {
        // Formato estándar (ISO 8601 Local): YYYY-MM-DDTHH:MM:SS
        strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    }
    return String(buf);
}

time_t TimeManager::getCurrentEpochTime() {
    if (!_timeSynchronized) {
        return 0; // Retorna 0 si no está sincronizado
    }
    time_t now;
    // Obtiene el tiempo (epoch) del RTC interno del ESP32 (ajustado por NTP).
    time(&now); 
    return now;
}

bool TimeManager::isTimeSynced() const {
    // Retorna el estado del flag de sincronización.
    return _timeSynchronized;
}