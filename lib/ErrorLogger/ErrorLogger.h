#ifndef ERRORLOGGER_H
#define ERRORLOGGER_H

#include <Arduino.h>

// Declaraciones anticipadas (Forward declarations)
// Evitan la necesidad de incluir los headers completos (SDManager.h, TimeManager.h)
// en este archivo .h, reduciendo dependencias y tiempos de compilación.
class SDManager;
class TimeManager;
enum class LogLevel;

// Definición de tipos de log estándar
extern const char LOG_TYPE_INFO[];
extern const char LOG_TYPE_WARNING[];
extern const char LOG_TYPE_ERROR[];

/**
 * @class ErrorLogger
 * @brief Clase de utilidad (estática) para gestionar el registro de eventos
 * tanto localmente (SD) como remotamente (API).
 */
class ErrorLogger {
public:
    /**
     * @brief Envía un mensaje de log localmente (SD) y opcionalmente a una API remota.
     *
     * La función SIEMPRE intenta guardar el log en la tarjeta SD primero.
     * Si la SD está disponible y el guardado es exitoso, y además hay conexión
     * WiFi y una URL de log, intentará enviar el log a la API.
     *
     * @param sdManager Referencia a la instancia de SDManager (para guardado local).
     * @param timeManager Referencia a la instancia de TimeManager (para timestamps).
     * @param fullLogUrl URL completa (String) del endpoint de la API de logs.
     * @param accessToken Token de acceso (String) para la cabecera `Authorization`.
     * @param logType Tipo de log (ej. LOG_TYPE_INFO, LOG_TYPE_ERROR).
     * @param logMessage El mensaje de log detallado (String).
     * @param internalTemp Opcional. Temperatura interna del dispositivo (float).
     *
     * @return `true` si el log se guardó exitosamente en la tarjeta SD.
     * `false` si falló el guardado en la SD.
     * @note El valor de retorno *no* indica si el envío remoto fue exitoso.
     */
    static bool sendLog(SDManager& sdManager,        
                       TimeManager& timeManager,      
                       const String& fullLogUrl, 
                       const String& accessToken, 
                       const char* logType, 
                       const String& logMessage, 
                       float internalTemp = NAN);

    /**
     * @brief Envía un mensaje de log *exclusivamente* a la tarjeta SD local.
     *
     * Este método no intenta ninguna comunicación de red. Es útil para
     * diagnósticos críticos locales o cuando la red no está disponible.
     *
     * @param sdManager Referencia a la instancia de SDManager.
     * @param timeManager Referencia a la instancia de TimeManager.
     * @param level Nivel de severidad (enum LogLevel de SDManager.h).
     * @param logMessage El mensaje de log detallado (String).
     * @param internalTemp Opcional. Temperatura interna del dispositivo (float).
     *
     * @return `true` si el log se guardó exitosamente en la SD.
     * `false` si falló el guardado.
     */
    static bool logToSdOnly(SDManager& sdManager,
                            TimeManager& timeManager,
                            LogLevel level,
                            const String& logMessage,
                            float internalTemp = NAN);
};

#endif // ERRORLOGGER_H