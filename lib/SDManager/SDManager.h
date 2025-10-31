#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include "FS.h"      // Clase base del sistema de archivos
#include "SD_MMC.h"  // Librería para SD Card (usando periférico SDMMC)
#include <vector>    // Para gestionar listas de archivos
#include <algorithm> // Requerido para std::sort en manageLogStorage 

// --- Dependencias de otros módulos ---
#include "API.h" 
#include "TimeManager.h" 
#include "ConfigManager.h" 
#include "ErrorLogger.h"   
#include "EnvironmentDataJSON.h" 
#include "MultipartDataSender.h" 

// Define los niveles de severidad para los logs
enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

// --- Estructura de Directorios Estándar ---
#define LOG_DIR "/logs"                 // Logs de eventos diarios
#define SECURE_DATA_DIR "/secure_data"  // Para estado de API (tokens)
#define PENDING_DATA_DIR "/data_pending" // Directorio raíz para datos que fallaron al enviar
#define AMBIENT_PENDING_DIR PENDING_DATA_DIR "/ambient" // Datos ambientales pendientes
#define CAPTURE_PENDING_DIR PENDING_DATA_DIR "/capture" // Datos de captura (térmica/visual) pendientes

// Directorios para archivo a largo plazo (datos enviados exitosamente)
#define ARCHIVE_DIR "/archive"
#define ARCHIVE_ENVIRONMENTAL_DIR ARCHIVE_DIR "/environmental"
#define ARCHIVE_CAPTURES_DIR ARCHIVE_DIR "/captures"

// Archivo de estado de la API
#define API_STATE_FILENAME SECURE_DATA_DIR "/api_state.json" 

class API; // Declaración anticipada

/**
 * @class SDManager
 * @brief Gestiona todas las interacciones con la tarjeta SD.
 * * Esta clase centraliza la inicialización, creación de directorios,
 * lectura/escritura de archivos, y maneja la lógica de negocio para:
 * 1. Almacenamiento de estado (tokens API).
 * 2. Registro de logs de eventos.
 * 3. Almacenamiento de datos pendientes (para reintentos).
 * 4. Procesamiento y reenvío de datos pendientes.
 * 5. Limpieza automática de almacenamiento (logs y archivos antiguos).
 */
class SDManager {
public:
    SDManager();

    /**
     * @brief Inicializa la tarjeta SD usando el periférico SD_MMC (modo 1-bit).
     * Intenta montar la tarjeta y crea la estructura de directorios base.
     * @return True si la SD está inicializada y disponible, false en caso contrario.
     */
    bool begin();

    /**
     * @brief Verifica si la tarjeta SD está disponible (si begin() fue exitoso).
     * @return True si la SD está lista, false en caso contrario.
     */
    bool isSDAvailable() const;

    /**
     * @brief Escribe un mensaje de log formateado en un archivo diario en la SD.
     * Los archivos se nombran /logs/YYYYMMDD_log.txt.
     * @param timestamp String con la fecha y hora.
     * @param level Nivel de severidad (INFO, WARNING, ERROR).
     * @param message El mensaje de log.
     * @param internalTemp Opcional. Temperatura interna del dispositivo.
     * @return True si la escritura fue exitosa, false en caso contrario.
     */
    bool logToFile(const String& timestamp, LogLevel level, const String& message, float internalTemp = NAN);

    /**
     * @brief Guarda el estado de la aplicación (ej. tokens API) en un archivo JSON.
     * @note Actualmente guarda en **texto plano**. La encriptación se puede añadir aquí.
     * @param stateJson El string JSON con el estado a guardar.
     * @return True si se guardó exitosamente, false en caso contrario.
     */
    bool saveApiState(const String& stateJson);

    /**
     * @brief Lee el estado de la aplicación desde un archivo JSON.
     * @note Actualmente lee **texto plano**.
     * @param stateJsonOut Referencia a un String donde se almacenará el JSON leído.
     * @return True si se leyó exitosamente, false si el archivo no existe o falla.
     */
    bool readApiState(String& stateJsonOut);

    /**
     * @brief Guarda datos de texto (ej. JSON ambiental) en el directorio de pendientes.
     * @param subDir Subdirectorio (ej. "ambient").
     * @param filename Nombre del archivo.
     * @param data Los datos (String) a guardar.
     * @return True si se guardó exitosamente.
     */
    bool savePendingTextData(const String& subDir, const String& filename, const String& data);

    /**
     * @brief Guarda datos binarios (ej. imagen JPEG) en el directorio de pendientes.
     * @param subDir Subdirectorio (ej. "capture").
     * @param filename Nombre del archivo.
     * @param data Puntero al buffer de datos binarios.
     * @param length Tamaño de los datos en bytes.
     * @return True si se guardó exitosamente.
     */
    bool savePendingBinaryData(const String& subDir, const String& filename, const uint8_t* data, size_t length);

     /**
     * @brief Escribe datos de texto en una ruta específica (sobrescribe si existe).
     * @param fullPath Ruta completa (ej. "/archive/environmental/data.json").
     * @param data Datos (String) a guardar.
     * @return True si la escritura fue exitosa.
     */
    bool writeTextFile(const String& fullPath, const String& data);

    /**
     * @brief Escribe datos binarios en una ruta específica (sobrescribe si existe).
     * @param fullPath Ruta completa (ej. "/archive/captures/image.jpg").
     * @param data Puntero al buffer de datos binarios.
     * @param length Tamaño de los datos en bytes.
     * @return True si la escritura fue exitosa.
     */
    bool writeBinaryFile(const String& fullPath, const uint8_t* data, size_t length);

    /**
     * @brief Mueve un archivo (usa `rename` de SD_MMC para eficiencia).
     * @param srcPath Ruta de origen completa.
     * @param destPath Ruta de destino completa.
     * @return True si el movimiento fue exitoso.
     */
    bool moveFile(const String& srcPath, const String& destPath);

    /**
     * @brief Lista recursivamente los archivos en un directorio (para depuración).
     * @param dirname Directorio a listar.
     * @param levels Niveles de recursión.
     */
    void listDir(const char* dirname, uint8_t levels);
    
    /**
     * @brief Borra un archivo en la ruta especificada.
     * @param path Ruta completa del archivo a borrar.
     * @return True si el borrado fue exitoso.
     */
    bool deleteFile(const char* path);

    /**
     * @brief Procesa los datos pendientes (ambientales y de captura) guardados en la SD.
     * * Itera sobre los directorios 'pending'. Intenta reenviar los datos usando
     * el objeto API. Si el envío es exitoso, mueve los archivos al directorio
     * 'archive' correspondiente.
     * * @param api_comm Referencia al objeto API (para tokens y URLs).
     * @param timeMgr Referencia al TimeManager (para logs).
     * @param cfg Referencia a la Configuración (para rutas de API).
     * @param internalTempForLog Temperatura interna para registrar los intentos.
     * @return True si se encontró y procesó (con éxito o no) algún dato pendiente.
     */
    bool processPendingApiCalls(API& api_comm, TimeManager& timeMgr, Config& cfg, float internalTempForLog = NAN);

    /**
     * @brief Gestiona el espacio de almacenamiento (logs y archivos).
     * * Borra archivos en `LOG_DIR` y `ARCHIVE_DIR` que sean más antiguos
     * que `maxFileAgeDays`, o si el espacio libre global cae por debajo
     * de `minFreeSpacePercentage` (borrando los más antiguos primero).
     * * @note Contiene una optimización para solo ejecutarse si el disco
     * supera un umbral de uso (ej. 90%).
     *
     * @param timeMgr Referencia al TimeManager (para obtener la fecha actual).
     * @param maxFileAgeDays Edad máxima de los archivos (días).
     * @param minFreeSpacePercentage Porcentaje mínimo de espacio libre a mantener.
     */
    void manageAllStorage(TimeManager& timeMgr, int maxFileAgeDays = 365, float minFreeSpacePercentage = 5.0f);

    /**
     * @brief Obtiene información de uso de la tarjeta SD.
     * @param[out] outUsedBytes Referencia a bytes usados.
     * @param[out] outTotalBytes Referencia a bytes totales.
     * @return Porcentaje de uso (0.0-100.0). -1.0f si falla.
     */
    float getUsageInfo(uint64_t& outUsedBytes, uint64_t& outTotalBytes);

    /**
     * @brief (Ayuda Web Portal) Lista archivos en un directorio y devuelve un JSON.
     * @param dirname Directorio a listar.
     * @return String con un array JSON (ej. ["log1.txt", "log2.txt"]).
     */
    String listLogFilesJSON(const char* dirname);

    /**
     * @brief (Ayuda Web Portal) Abre un archivo de log para lectura (streaming).
     * @param path Ruta completa del archivo.
     * @return Objeto File. El llamador debe cerrarlo. (Evalúa a 'false' si falla).
     */
    File getLogFile(const String& path);

private:
    bool _sdAvailable; // Flag de estado de inicialización

    // Estructura para ayudar a ordenar archivos por fecha
    struct FileInfo {
        String path;
        time_t timestamp; // Epoch time para ordenar fácilmente

        bool operator<(const FileInfo& other) const {
            return timestamp < other.timestamp; // Ordenar: más antiguo primero
        }
    };

    /**
     * @brief (Helper) Asegura que una ruta de directorio exista, creándola si es necesario.
     * @param path Ruta del directorio.
     * @return True si existe o fue creado.
     */
    bool ensureDirectoryExists(const char* path);

    /**
     * @brief (Helper) Convierte el enum LogLevel a String.
     */
    String logLevelToString(LogLevel level);

    /**
     * @brief (Helper) Lógica de gestión para un único directorio (borrado por antigüedad/espacio).
     * @return Bytes liberados en esta ejecución.
     */
    uint64_t _manageDirectory(const String& dirPath, TimeManager& timeMgr, int maxFileAgeDays, uint64_t minTotalFreeBytes, uint64_t& currentTotalUsedBytes, uint64_t totalSdSizeBytes);
    
    /**
     * @brief (Helper) Parsea un timestamp (epoch) desde un nombre de archivo (Formato: YYYYMMDD...).
     * @return Epoch time, o 0 si falla el parseo.
     */
    time_t _parseTimestampFromFilename(const String& filename, TimeManager& timeMgr);

    /**
     * @brief (Helper) Parsea un JSON térmico (String) y extrae el array de temperaturas.
     * @note El llamador es responsable de liberar la memoria del float* retornado.
     * @param jsonContent El string JSON.
     * @return Puntero a un array de 768 floats (alojado con malloc), o nullptr si falla.
     */
    float* parseThermalJson(const String& jsonContent);

    /**
     * @brief (Helper) Mueve un archivo a 'archive'. Si falla, lo borra de 'pending'.
     * Esto evita reintentos infinitos de archivos ya enviados cuyo movimiento falló.
     * @param srcPath Ruta de origen (en 'pending').
     * @param destPath Ruta de destino (en 'archive').
     */
    void archiveFile(const String& srcPath, const String& destPath);
};

#endif // SD_MANAGER_H