#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include "time.h" // Librería estándar C para manejo de tiempo

// Servidores NTP por defecto
#define DEFAULT_NTP_SERVER_1 "pool.ntp.org"
#define DEFAULT_NTP_SERVER_2 "time.nist.gov"

/**
 * @class TimeManager
 * @brief Gestiona la sincronización de tiempo (NTP) y la zona horaria para el ESP32.
 */
class TimeManager {
public:
    /**
     * @brief Constructor.
     */
    TimeManager();

    /**
     * @brief Inicializa el cliente NTP con servidores y zona horaria.
     * Llama a esta función una vez en el setup(), después de conectar al WiFi.
     *
     * @param ntpServer1 Servidor NTP primario.
     * @param ntpServer2 Servidor NTP secundario.
     * @param gmtOffset_sec Desplazamiento GMT en segundos (ej. -5 * 3600 para Bogotá, Colombia).
     * @param daylightOffset_sec Desplazamiento de horario de verano (normalmente 0).
     */
    void begin(const String& ntpServer1 = DEFAULT_NTP_SERVER_1, 
               const String& ntpServer2 = DEFAULT_NTP_SERVER_2, 
               long gmtOffset_sec = -5 * 3600, // UTC-5 por defecto
               int daylightOffset_sec = 0);

    /**
     * @brief Intenta sincronizar la hora local con un servidor NTP.
     * Esta función es bloqueante y puede tardar varios segundos.
     *
     * @return True si la sincronización fue exitosa, false en caso contrario.
     */
    bool syncNtpTime();

    /**
     * @brief Obtiene el timestamp actual formateado como un String.
     *
     * @param forFileNames 
     * - `true`: Retorna formato "YYYYMMDD_HHMMSS" (ideal para nombres de archivo).
     * - `false`: Retorna formato "YYYY-MM-DDTHH:MM:SS" (ISO 8601 local, para logs).
     *
     * @return String con la hora. Si la hora no está sincronizada,
     * retorna un string basado en millis() (ej. "UPTIME_00h15m30s").
     */
    String getCurrentTimestampString(bool forFileNames = false);

    /**
     * @brief Obtiene la hora actual como segundos desde "epoch" (Tiempo Unix).
     *
     * @return Epoch time (time_t). Retorna 0 si la hora no está sincronizada.
     */
    time_t getCurrentEpochTime();

    /**
     * @brief Verifica si la hora ha sido sincronizada exitosamente al menos una vez.
     *
     * @return True si la hora está sincronizada, false en caso contrario.
     */
    bool isTimeSynced() const;

private:
    ///< Indica si la sincronización NTP ha sido exitosa al menos una vez.
    bool _timeSynchronized; 
    
    // Almacenamiento local de la configuración NTP
    String _ntpServer1;
    String _ntpServer2;
    long _gmtOffset_sec;
    int _daylightOffset_sec;

    // Constantes para los reintentos de sincronización
    static const int NTP_SYNC_MAX_RETRIES = 5;
    static const unsigned long NTP_SYNC_RETRY_DELAY_MS = 1000; 
};

#endif // TIME_MANAGER_H