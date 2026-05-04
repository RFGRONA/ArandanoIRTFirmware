#include "SDManager.h"
#include "TimeManager.h" 
#include <ArduinoJson.h>

// --- Pines para SD_MMC (Modo 1-bit) ---
#define SD_CARD_MMC_CLK_PIN 39
#define SD_CARD_MMC_CMD_PIN 38
#define SD_CARD_MMC_D0_PIN  40

SDManager::SDManager() : _sdAvailable(false) {
    // Constructor
}

bool SDManager::begin() {
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager] Initializing SD Card (SD_MMC 1-bit mode)..."));
        Serial.printf("[SDManager] Using PINS: CLK=%d, CMD=%d, D0=%d\n", SD_CARD_MMC_CLK_PIN, SD_CARD_MMC_CMD_PIN, SD_CARD_MMC_D0_PIN);
    #endif

     // Configura los pines para el periférico SD_MMC
     SD_MMC.setPins(SD_CARD_MMC_CLK_PIN, SD_CARD_MMC_CMD_PIN, SD_CARD_MMC_D0_PIN);
    
     // Intenta inicializar en modo 1-bit (tercer argumento 'true')
    if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT)) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager] SD_MMC.begin failed. Card Mount Failed or no card present."));
        #endif
        _sdAvailable = false;
        return false;
    }

    uint8_t cardType = SD_MMC.cardType();
    if (cardType == CARD_NONE) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager] No SD card attached."));
        #endif
        _sdAvailable = false;
        SD_MMC.end(); 
        return false;
    }

    // (Logs de depuración sobre el tipo y tamaño de la tarjeta)

    _sdAvailable = true; 

    // --- Crear estructura de directorios base ---
    // Si falla la creación de cualquier directorio esencial, marca la SD como no disponible.
    if (!ensureDirectoryExists(LOG_DIR)) _sdAvailable = false;
    if (!ensureDirectoryExists(SECURE_DATA_DIR)) _sdAvailable = false;
    if (!ensureDirectoryExists(PENDING_DATA_DIR)) _sdAvailable = false;
    if (_sdAvailable && !ensureDirectoryExists(AMBIENT_PENDING_DIR)) _sdAvailable = false;
    if (_sdAvailable && !ensureDirectoryExists(CAPTURE_PENDING_DIR)) _sdAvailable = false; 
    if (_sdAvailable && !ensureDirectoryExists(ARCHIVE_DIR)) _sdAvailable = false;
    if (_sdAvailable && !ensureDirectoryExists(ARCHIVE_ENVIRONMENTAL_DIR)) _sdAvailable = false;
    if (_sdAvailable && !ensureDirectoryExists(ARCHIVE_CAPTURES_DIR)) _sdAvailable = false;

    if (!_sdAvailable) { 
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager] CRITICAL: Failed to create one or more essential directories. SD operations might fail."));
        #endif
        return false; 
    }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager] SD Card initialized successfully and directories checked/created."));
    #endif
    return true; 
}


bool SDManager::isSDAvailable() const {
    return _sdAvailable;
}

// (Helper para convertir LogLevel a String)
String SDManager::logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

float SDManager::getUsageInfo(uint64_t& outUsedBytes, uint64_t& outTotalBytes) {
    outUsedBytes = 0;
    outTotalBytes = 0;
    if (!_sdAvailable) return -1.0f;

    outTotalBytes = SD_MMC.totalBytes();
    outUsedBytes = SD_MMC.usedBytes();

    if (outTotalBytes == 0) return -1.0f; // Evitar división por cero

    float usagePercentage = ((float)outUsedBytes / outTotalBytes) * 100.0f;
    return usagePercentage;
}

bool SDManager::logToFile(const String& timestamp, LogLevel level, const String& message, float internalTemp) {
    if (!_sdAvailable) return false;

    // Extrae la fecha (ej. "2025-10-31") y la convierte a "20251031"
    String datePart = timestamp.substring(0, 10); 
    datePart.remove(7, 1); // Quita el segundo '-'
    datePart.remove(4, 1); // Quita el primer '-'
    String dailyLogFilename = String(LOG_DIR) + "/" + datePart + "_log.txt";

    // Abre el archivo en modo "append" (añadir al final)
    File logFile = SD_MMC.open(dailyLogFilename.c_str(), FILE_APPEND);
    if (!logFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open log file for appending: " + dailyLogFilename);
        #endif
        return false;
    }

    String logEntry = timestamp + " [" + logLevelToString(level) + "] " + message;
    // Añade la temperatura interna si se proporciona un valor válido
    if (!isnan(internalTemp)) {
        logEntry += " (DevTemp: " + String(internalTemp, 1) + "C )";
    }

    logFile.println(logEntry);
    logFile.close();
    return true;
}

bool SDManager::saveApiState(const String& stateJson) {
    if (!_sdAvailable) return false;

    // --- IMPLEMENTACIÓN EN TEXTO PLANO ---
    // (La encriptación se añadiría aquí si fuese necesario)
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Saving API state (plain text): " + String(API_STATE_FILENAME));
    #endif

    // Abre en modo escritura (FILE_WRITE), sobrescribiendo el contenido anterior
    File stateFile = SD_MMC.open(API_STATE_FILENAME, FILE_WRITE);
    if (!stateFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open API state file for writing.");
        #endif
        return false;
    }
    stateFile.print(stateJson);
    stateFile.close();
    return true;
}

bool SDManager::readApiState(String& stateJsonOut) {
    if (!_sdAvailable) return false;
    stateJsonOut = "";

    // --- IMPLEMENTACIÓN EN TEXTO PLANO ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Reading API state (plain text): " + String(API_STATE_FILENAME));
    #endif

    if (!SD_MMC.exists(API_STATE_FILENAME)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] API state file does not exist.");
        #endif
        return false; // No es un error, solo indica que no hay estado guardado.
    }

    File stateFile = SD_MMC.open(API_STATE_FILENAME, FILE_READ);
    if (!stateFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open API state file for reading.");
        #endif
        return false;
    }
    stateJsonOut = stateFile.readString();
    stateFile.close();
    return !stateJsonOut.isEmpty();
}

bool SDManager::writeTextFile(const String& fullPath, const String& data) {
    if (!_sdAvailable) return false;
    
    // FILE_WRITE: Crea si no existe, sobrescribe (trunca) si existe.
    File file = SD_MMC.open(fullPath.c_str(), FILE_WRITE);
    if (!file) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open text file for writing: " + fullPath);
        #endif
        return false;
    }
    size_t bytesWritten = file.print(data);
    file.close();

    // Verifica que se haya escrito todo el contenido
    if (bytesWritten != data.length()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Error: Not all text data written to file: " + fullPath);
        #endif
        return false;
    }
    return true;
}

bool SDManager::writeBinaryFile(const String& fullPath, const uint8_t* data, size_t length) {
    if (!_sdAvailable) return false;

    File file = SD_MMC.open(fullPath.c_str(), FILE_WRITE);
    if (!file) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open binary file for writing: " + fullPath);
        #endif
        return false;
    }
    size_t bytesWritten = file.write(data, length);
    file.close();

    if (bytesWritten != length) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Error: Not all binary data written to file: " + fullPath);
        #endif
        return false;
    }
    return true;
}

// (Wrapper para guardar en directorio 'pending/ambient')
bool SDManager::savePendingTextData(const String& subDir, const String& filename, const String& data) {
    if (!_sdAvailable) return false;
    // Asegura que el subdirectorio (ej. 'pending/ambient') exista
    if (!ensureDirectoryExists((String(PENDING_DATA_DIR) + "/" + subDir).c_str())) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to ensure pending subdirectory exists: " + String(PENDING_DATA_DIR) + "/" + subDir);
        #endif
        return false;
    }
    String path = String(PENDING_DATA_DIR) + "/" + subDir + "/" + filename;
    return writeTextFile(path, data);
}

// (Wrapper para guardar en directorio 'pending/capture')
bool SDManager::savePendingBinaryData(const String& subDir, const String& filename, const uint8_t* data, size_t length) {
    if (!_sdAvailable) return false;
     if (!ensureDirectoryExists((String(PENDING_DATA_DIR) + "/" + subDir).c_str())) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to ensure pending subdirectory exists: " + String(PENDING_DATA_DIR) + "/" + subDir);
        #endif
        return false;
    }
    String path = String(PENDING_DATA_DIR) + "/" + subDir + "/" + filename;
    return writeBinaryFile(path, data, length);
}


bool SDManager::moveFile(const String& srcPath, const String& destPath) {
    if (!_sdAvailable) return false;

    if (!SD_MMC.exists(srcPath.c_str())) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] moveFile: Source file does not exist: " + srcPath);
        #endif
        return false;
    }

    // Asegura que el directorio de destino exista antes de mover
    int lastSlash = destPath.lastIndexOf('/');
    if (lastSlash > 0) {
        String destDir = destPath.substring(0, lastSlash);
        if (!ensureDirectoryExists(destDir.c_str())) {
             #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SDManager] moveFile: Failed to ensure destination directory exists: " + destDir);
            #endif
            return false;
        }
    }

    // `rename` es el método de SD_MMC para mover archivos eficientemente
    if (SD_MMC.rename(srcPath.c_str(), destPath.c_str())) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] File moved successfully from " + srcPath + " to " + destPath);
        #endif
        return true;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] moveFile: Failed to rename/move file from " + srcPath + " to " + destPath);
        #endif
        return false;
    }
}

// (Helper para crear directorios)
bool SDManager::ensureDirectoryExists(const char* path) {
    if (!SD_MMC.exists(path)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager] Directory %s does not exist. Creating...\n", path);
        #endif
        if (!SD_MMC.mkdir(path)) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.printf("[SDManager] Failed to create directory: %s\n", path);
            #endif
            return false;
        }
    }
    return true;
}

// (Helper de depuración para listar directorios)
void SDManager::listDir(const char * dirname, uint8_t levels){
    // (Implementación de listado recursivo para depuración)
}

bool SDManager::deleteFile(const char* path) {
    if (!_sdAvailable) return false;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SDManager] Deleting file: %s\n", path);
    #endif
    if (SD_MMC.remove(path)) {
        return true;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to delete file.");
        #endif
        return false;
    }
}

// (Helper para Web Portal: Listar logs como JSON)
String SDManager::listLogFilesJSON(const char* dirname) {
    String jsonOutput = "[]"; 
    if (!_sdAvailable) return jsonOutput;

    File root = SD_MMC.open(dirname);
    if (!root || !root.isDirectory()) {
        if(root) root.close();
        return jsonOutput;
    }

    DynamicJsonDocument doc(1024); // Capacidad para la lista de archivos
    JsonArray filesArray = doc.to<JsonArray>();

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            filesArray.add(file.name());
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();

    serializeJson(doc, jsonOutput);
    return jsonOutput;
}

// (Helper para Web Portal: Obtener archivo de log para streaming)
File SDManager::getLogFile(const String& path) {
    if (!_sdAvailable) {
        return File(); // Devuelve objeto File inválido
    }

    File file = SD_MMC.open(path.c_str(), FILE_READ);
    
    // Asegura que sea un archivo y no un directorio
    if (!file || file.isDirectory()) {
        if(file) file.close(); 
        return File(); // Devuelve objeto File inválido
    }
    
    // Devuelve el archivo abierto. El portal web debe cerrarlo.
    return file;
}

// --- Helpers Estáticos para Lectura de Archivos (usados en processPendingApiCalls) ---

// (Lee un archivo de texto completo a un String)
static String readFileToString(const char* path) {
    if (!SD_MMC.exists(path)) return "";
    File file = SD_MMC.open(path);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return "";
    }
    String content = file.readString();
    file.close();
    return content;
}

// (Lee un archivo binario completo a un buffer. El llamador debe hacer free())
static uint8_t* readBinaryFileToBuffer(const char* path, size_t& fileSize) {
    fileSize = 0;
    if (!SD_MMC.exists(path)) return nullptr;
    File file = SD_MMC.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        if (file) file.close();
        return nullptr;
    }
    
    fileSize = file.size();
    if (fileSize == 0) {
        file.close();
        return nullptr;
    }

    uint8_t* buffer = (uint8_t*)malloc(fileSize); // Aloca en HEAP (o PSRAM si malloc está configurado)
    if (!buffer) {
        file.close();
        return nullptr;
    }

    size_t bytesRead = file.read(buffer, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        free(buffer);
        return nullptr;
    }
    return buffer;
}


// --- Lógica de Negocio Principal ---

bool SDManager::processPendingApiCalls(API& api_comm, TimeManager& timeMgr, Config& cfg, float internalTempForLog) {
    // Condiciones de salida: No reintentar si no hay SD, WiFi, o activación.
    if (!_sdAvailable || !api_comm.isActivated() || WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            // (Logs de depuración explicando por qué se omite el proceso)
        #endif
        return false;
    }

    bool workDone = false; // Flag para saber si se hizo algún trabajo
    String logUrl = api_comm.getBaseApiUrl() + cfg.apiLogPath; // Para logs remotos

    // --- 1. Procesar Datos Ambientales Pendientes (ambient_pending) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager_Pending] Checking for pending ambient data..."));
    #endif
    File ambientPendingDir = SD_MMC.open(AMBIENT_PENDING_DIR);
    if (ambientPendingDir && ambientPendingDir.isDirectory()) {
        File entry = ambientPendingDir.openNextFile();
        while (entry) {
            // Solo procesa archivos .json que no sean directorios
            if (!entry.isDirectory() && String(entry.name()).endsWith("_env.json")) {
                workDone = true;
                String filePath = String(entry.path());
                String fileNameOnly = String(entry.name());
                entry.close(); // Cierra el handle del archivo (importante!)

                String jsonData = readFileToString(filePath.c_str());
                if (!jsonData.isEmpty()) {
                    // Re-parsea el JSON para obtener los valores individuales
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, jsonData);
                    if (!error) {
                        String timestamp = doc["timestamp"] | "0000-00-00_00:00:00";
                        float light = doc["light"] | NAN; 
                        float temp = doc["temperature"] | NAN;
                        float hum = doc["humidity"] | NAN;
                        float press = doc["pressure"] | NAN;

                        // Intenta el reenvío usando la función de envío original
                        String targetApiUrl = api_comm.getBaseApiUrl() + cfg.apiAmbientDataPath;
                        int httpCode = EnvironmentDataJSON::IOEnvironmentData(targetApiUrl, api_comm.getAccessToken(), timestamp, light, temp, hum, press);

                        if (httpCode == 200 || httpCode == 204) {
                            // Éxito: Mover a 'archive'
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::INFO, "Sent pending ambient data: " + fileNameOnly, internalTempForLog);
                            String archivePath = String(ARCHIVE_ENVIRONMENTAL_DIR) + "/" + fileNameOnly;
                            archiveFile(filePath, archivePath);
                        } else if (httpCode == 401) {
                            // Error de Auth: No hacer nada, esperar refresco de token
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Auth error sending pending ambient: " + fileNameOnly + ". HTTP: " + String(httpCode), internalTempForLog);
                        } else {
                            // Otro error (500, timeout, etc): Reintentar en la próxima vuelta
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed send pending ambient: " + fileNameOnly + ". HTTP: " + String(httpCode), internalTempForLog);
                        }
                    } else {
                        // Error de parseo: JSON corrupto, no se puede reenviar
                        ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Failed to parse pending ambient JSON: " + fileNameOnly, internalTempForLog);
                        // (Considerar mover a un directorio "corrupto")
                    }
                } else {
                     ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Empty/unreadable pending ambient file: " + fileNameOnly, internalTempForLog);
                }
                entry = ambientPendingDir.openNextFile(); // Siguiente archivo
            } else {
                entry.close(); // Cierra el directorio o archivo no procesado
                entry = ambientPendingDir.openNextFile(); // Siguiente entrada
            }
        }
        if(entry) entry.close(); 
        ambientPendingDir.close();
    }

    // --- 2. Procesar Datos de Captura Pendientes (capture_pending) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager_Pending] Checking for pending capture data..."));
    #endif
    File capturePendingDir = SD_MMC.open(CAPTURE_PENDING_DIR);
    if (!capturePendingDir) return false;
    
    // Estrategia: Obtener todos los archivos JSON térmicos primero
    std::vector<String> thermalJsonFiles;
    File entryCap = capturePendingDir.openNextFile();
    while(entryCap){
        if(!entryCap.isDirectory() && String(entryCap.name()).endsWith("_thermal.json")){
            thermalJsonFiles.push_back(String(entryCap.path()));
        }
        entryCap.close(); 
        entryCap = capturePendingDir.openNextFile();
    }
    capturePendingDir.close();

    // Itera sobre los archivos JSON encontrados
    for (const String& thermalJsonPath : thermalJsonFiles) {
        workDone = true;
        // Extrae el nombre base (ej. "20251031_100000")
        String baseName = thermalJsonPath.substring(thermalJsonPath.lastIndexOf('/') + 1);
        baseName = baseName.substring(0, baseName.indexOf("_thermal.json"));
        // Construye las rutas esperadas
        String visualJpgPath = String(CAPTURE_PENDING_DIR) + "/" + baseName + "_visual.jpg";
        String thermalFileNameOnly = baseName + "_thermal.json";
        
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Pending] Processing thermal file: " + thermalFileNameOnly);
        #endif

        // Diferenciar entre par completo o archivo térmico "huérfano"
        if (SD_MMC.exists(visualJpgPath.c_str())) {
            // --- CASO 1: Par completo (Térmica + Visual) ---
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SDManager_Pending]   -> Visual counterpart found. Processing as a pair.");
            #endif
            
            String visualFileNameOnly = baseName + "_visual.jpg";
            String thermalJsonContent = readFileToString(thermalJsonPath.c_str());
            size_t jpegLength = 0;
            uint8_t* jpegImage = readBinaryFileToBuffer(visualJpgPath.c_str(), jpegLength);

            // Extraer timestamp del JSON térmico
            JsonDocument doc;
            String timestamp = "0000-00-00_00:00:00";
            DeserializationError error = deserializeJson(doc, thermalJsonContent);
            if (!error) timestamp = doc["timestamp"] | timestamp;

            float* thermalDataArray = parseThermalJson(thermalJsonContent);

            // VERIFICACIÓN: Si el JSON térmico o la imagen están corruptos/ilegibles
            if (!thermalDataArray || !jpegImage) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Corrupted pending pair: " + baseName + ". Deleting.", internalTempForLog);
                deleteFile(thermalJsonPath.c_str());
                deleteFile(visualJpgPath.c_str());
                free(jpegImage); // Libera si solo uno de los dos falló
                free(thermalDataArray);
                continue; // Siguiente archivo
            }

            int httpCode = MultipartDataSender::IOThermalAndImageData(
                api_comm.getBaseApiUrl() + cfg.apiCaptureDataPath,
                api_comm.getAccessToken(),
                timestamp, 
                thermalDataArray,
                jpegImage,
                jpegLength
            );

            if (httpCode >= 200 && httpCode < 300) { // Éxito
                archiveFile(thermalJsonPath, String(ARCHIVE_CAPTURES_DIR) + "/" + thermalFileNameOnly);
                archiveFile(visualJpgPath, String(ARCHIVE_CAPTURES_DIR) + "/" + visualFileNameOnly);
            } else { // Fallo (Auth, Server Error, etc)
                 ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed to send pending pair " + baseName + ", HTTP: " + String(httpCode), internalTempForLog);
            }
            free(thermalDataArray);
            free(jpegImage);

        } else {
            // --- CASO 2: Archivo térmico huérfano (sin contraparte visual) ---
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SDManager_Pending]   -> No visual counterpart. Processing as thermal-only.");
            #endif

            String thermalJsonContent = readFileToString(thermalJsonPath.c_str());
            if (thermalJsonContent.isEmpty()) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Unreadable pending thermal-only file: " + thermalFileNameOnly + ". Deleting.", internalTempForLog);
                deleteFile(thermalJsonPath.c_str());
                continue; 
            }

            JsonDocument doc;
            String timestamp = "0000-00-00_00:00:00";
            DeserializationError error = deserializeJson(doc, thermalJsonContent);
            if (!error) timestamp = doc["timestamp"] | timestamp;

            float* thermalDataArray = parseThermalJson(thermalJsonContent);
            if (!thermalDataArray) { // JSON Corrupto
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Corrupted pending thermal-only JSON: " + thermalFileNameOnly + ". Deleting.", internalTempForLog);
                deleteFile(thermalJsonPath.c_str());
                continue;
            }
            
            // Llama a la misma función, pero con 'nullptr' para la imagen
            int httpCode = MultipartDataSender::IOThermalAndImageData(
                api_comm.getBaseApiUrl() + cfg.apiCaptureDataPath,
                api_comm.getAccessToken(),
                timestamp,
                thermalDataArray,
                nullptr, // Sin imagen
                0        // Sin tamaño
            );

            if (httpCode >= 200 && httpCode < 300) { // Éxito
                archiveFile(thermalJsonPath, String(ARCHIVE_CAPTURES_DIR) + "/" + thermalFileNameOnly);
            } else { // Fallo
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed to send pending thermal-only " + baseName + ", HTTP: " + String(httpCode), internalTempForLog);
            }
            free(thermalDataArray);
        }
    }
    
    return workDone;
}

// (Helper para parsear YYYYMMDD... de nombres de archivo a epoch time)
time_t SDManager::_parseTimestampFromFilename(const String& filename, TimeManager& timeMgr) {
    struct tm t{};
    if (filename.length() < 8) return 0; 

    // Parsea solo la fecha (YYYYMMDD) del inicio del nombre
    if (sscanf(filename.c_str(), "%4d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
        t.tm_year -= 1900; // tm_year es años desde 1900
        t.tm_mon -= 1;     // tm_mon es 0-11
        t.tm_hour = 0; 
        t.tm_min = 0; 
        t.tm_sec = 0;
        t.tm_isdst = -1; // Dejar que mktime determine DST
        return mktime(&t); // Convierte struct tm a epoch time
    }
    return 0; // Falla de parseo
}

// (Helper: Lógica de limpieza para un solo directorio)
uint64_t SDManager::_manageDirectory(const String& dirPath, TimeManager& timeMgr, int maxFileAgeDays, 
                                   uint64_t minTotalFreeBytes, uint64_t& currentTotalUsedBytes, uint64_t totalSdSizeBytes) {
    if (!_sdAvailable) return 0;

    File root = SD_MMC.open(dirPath.c_str());
    if (!root || !root.isDirectory()) {
        if(root) root.close();
        return 0;
    }

    // 1. Recopilar todos los archivos y sus timestamps
    std::vector<FileInfo> files;
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String path = String(entry.path()); 
            String name = String(entry.name());
            time_t timestamp = _parseTimestampFromFilename(name, timeMgr);
            if (timestamp > 0) { // Solo gestiona archivos con timestamp parseable
                files.push_back({path, timestamp});
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    if (files.empty()) return 0;
    
    // 2. Ordenar archivos (el más antiguo primero)
    std::sort(files.begin(), files.end());

    uint64_t bytesFreedThisRun = 0;
    time_t currentTime = timeMgr.getCurrentEpochTime(); 
    time_t maxAgeSeconds = (time_t)maxFileAgeDays * 24 * 60 * 60;

    // 3. Fase 1: Borrar archivos más antiguos que maxFileAgeDays
    #ifdef ENABLE_DEBUG_SERIAL
        int filesDeletedByAge = 0;
    #endif
    for (auto it = files.begin(); it != files.end(); /* no incrementar aquí */) {
        bool removed = false;
        // Solo borra por antigüedad si tenemos una hora actual válida
        if (currentTime > 0 && (currentTime - it->timestamp) > maxAgeSeconds) {
            File f = SD_MMC.open(it->path.c_str());
            uint64_t fileSize = f ? f.size() : 0;
            if(f) f.close();
            
            if (deleteFile(it->path.c_str())) {
                bytesFreedThisRun += fileSize;
                currentTotalUsedBytes -= fileSize; // Actualiza el contador global
                #ifdef ENABLE_DEBUG_SERIAL
                    filesDeletedByAge++;
                #endif
                it = files.erase(it); // Borra del vector y avanza el iterador
                removed = true;
            }
        }
        if (!removed) {
            ++it; // Avanza el iterador si no se borró
        }
    }
    // (Logs de depuración)

    // 4. Fase 2: Si aún falta espacio, borra más (empezando por los más antiguos)
    #ifdef ENABLE_DEBUG_SERIAL
        int filesDeletedBySpace = 0;
    #endif
    // Itera sobre los archivos restantes (ya ordenados, más antiguos primero)
    for (auto it = files.begin(); it != files.end(); /* no incrementar aquí */) {
        // Comprueba si el espacio libre global es menor que el mínimo requerido
        if ((totalSdSizeBytes - currentTotalUsedBytes) < minTotalFreeBytes) {
            File f = SD_MMC.open(it->path.c_str());
            uint64_t fileSize = f ? f.size() : 0;
            if(f) f.close();

            if (deleteFile(it->path.c_str())) {
                bytesFreedThisRun += fileSize;
                currentTotalUsedBytes -= fileSize; // Actualiza el contador global
                #ifdef ENABLE_DEBUG_SERIAL
                    filesDeletedBySpace++;
                #endif
                it = files.erase(it); // Borra y avanza
            } else {
                ++it; // Si falla el borrado, avanza para evitar bucle infinito
            }
        } else {
            break; // Se ha liberado suficiente espacio
        }
    }
    // (Logs de depuración)
    
    return bytesFreedThisRun;
}


void SDManager::manageAllStorage(TimeManager& timeMgr, int maxFileAgeDays, float minFreeSpacePercentage) {
    if (!_sdAvailable) return;

    // --- OPTIMIZACIÓN: Evitar escaneo costoso si el disco no está lleno ---
    uint64_t totalBytes = SD_MMC.totalBytes();
    if (totalBytes == 0) return;
    
    uint64_t usedBytes = SD_MMC.usedBytes();
    float usagePercent = ((float)usedBytes / totalBytes) * 100.0f;

    // Define un umbral para activar la limpieza (ej. 90%)
    const float CLEANUP_TRIGGER_PERCENTAGE = 90.0f; 

    // Si el uso es menor que el umbral, salta la limpieza.
    if (usagePercent < CLEANUP_TRIGGER_PERCENTAGE) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ManageAll] Disk usage is at %.2f%% (below %.0f%% threshold). Skipping full storage management scan.\n", usagePercent, CLEANUP_TRIGGER_PERCENTAGE);
        #endif
        return; 
    }
    // --- FIN DE LA OPTIMIZACIÓN ---

    // Si llegamos aquí, el disco está > 90% lleno y necesita limpieza.
    uint64_t minFreeBytesAbsolute = (uint64_t)(totalBytes * (minFreeSpacePercentage / 100.0f));

    // (Logs de depuración)

    // 'usedBytes' se pasa por referencia y se actualiza en cada llamada
    _manageDirectory(LOG_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);
    _manageDirectory(ARCHIVE_ENVIRONMENTAL_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);
    _manageDirectory(ARCHIVE_CAPTURES_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);

    // (Logs de depuración finales)
}

// (Helper para parsear el JSON térmico guardado y extraer el array de floats)
float* SDManager::parseThermalJson(const String& jsonContent) {
    JsonDocument thermalDoc;
    DeserializationError error = deserializeJson(thermalDoc, jsonContent);
    if (error) return nullptr;

    JsonArray temps = thermalDoc["temperatures"].as<JsonArray>();
    // Verifica que el array exista y tenga el tamaño correcto (768)
    if (temps.isNull() || temps.size() != MultipartDataSender::THERMAL_PIXELS) {
        return nullptr;
    }

    // Aloja memoria para el array de floats
    float* thermalDataArray = (float*)malloc(MultipartDataSender::THERMAL_PIXELS * sizeof(float));
    if (!thermalDataArray) return nullptr; // Falla de alocación

    // Copia los valores
    for (int i = 0; i < MultipartDataSender::THERMAL_PIXELS; ++i) {
        thermalDataArray[i] = temps[i].as<float>();
    }
    return thermalDataArray;
}

// (Helper para archivar o borrar archivos pendientes procesados)
void SDManager::archiveFile(const String& srcPath, const String& destPath) {
    if (moveFile(srcPath, destPath)) {
        // Éxito
    } else {
        // Si mover falla (ej. error de SD), borra el original para
        // evitar que se reenvíe un dato que ya fue aceptado por la API.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Pending] Failed to move to archive. Deleting from pending: " + srcPath);
        #endif
        deleteFile(srcPath.c_str());
    }
}