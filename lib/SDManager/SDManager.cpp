// lib/SDManager/SDManager.cpp
#include "SDManager.h"
#include "TimeManager.h" // Required for timeMgr

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

     SD_MMC.setPins(SD_CARD_MMC_CLK_PIN, SD_CARD_MMC_CMD_PIN, SD_CARD_MMC_D0_PIN);
    
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

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print(F("[SDManager] SD Card Type: "));
        if (cardType == CARD_MMC) Serial.println(F("MMC"));
        else if (cardType == CARD_SD) Serial.println(F("SDSC"));
        else if (cardType == CARD_SDHC) Serial.println(F("SDHC"));
        else Serial.println(F("UNKNOWN"));
        uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
        Serial.printf("[SDManager] SD Card Size: %lluMB\n", cardSize);
    #endif

    _sdAvailable = true; 

    // Create base directories
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
    return true; // Successfully mounted and essential directories created
}


bool SDManager::isSDAvailable() const {
    return _sdAvailable;
}

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

    if (!_sdAvailable) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager_Usage] SD not available."));
        #endif
        return -1.0f;
    }

    outTotalBytes = SD_MMC.totalBytes();
    outUsedBytes = SD_MMC.usedBytes();

    if (outTotalBytes == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager_Usage] SD Card total size is 0."));
        #endif
        return -1.0f; // Avoid division by zero
    }

    float usagePercentage = ((float)outUsedBytes / outTotalBytes) * 100.0f;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SDManager_Usage] Total: %llu B, Used: %llu B (%.2f%%)\n", outTotalBytes, outUsedBytes, usagePercentage);
    #endif

    return usagePercentage;
}

bool SDManager::logToFile(const String& timestamp, LogLevel level, const String& message, float internalTemp, float internalHum) {
    if (!_sdAvailable) return false;

    String datePart = timestamp.substring(0, 10); 
    datePart.remove(7, 1); 
    datePart.remove(4, 1); 
    String dailyLogFilename = String(LOG_DIR) + "/" + datePart + "_log.txt";

    File logFile = SD_MMC.open(dailyLogFilename.c_str(), FILE_APPEND);
    if (!logFile) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open log file for appending: " + dailyLogFilename);
        #endif
        return false;
    }

    String logEntry = timestamp + " [" + logLevelToString(level) + "] " + message;
    if (!isnan(internalTemp)) {
        logEntry += " (DevTemp: " + String(internalTemp, 1) + "C";
        if (!isnan(internalHum)) {
            logEntry += ", DevHum: " + String(internalHum, 0) + "%";
        }
        logEntry += ")";
    }

    logFile.println(logEntry);
    logFile.close();
    return true;
}

bool SDManager::saveApiState(const String& stateJson) {
    if (!_sdAvailable) return false;

    // --- PLAIN TEXT IMPLEMENTATION (Encryption to be added later) ---
    // To add encryption:
    // 1. Define an encryption key (e.g., from NVS, compiled-in, or derived).
    // 2. Encrypt 'stateJson' using ESP32 AES hardware or mbedTLS.
    // 3. Save the encrypted data.
    // For now, we save plain text.
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Saving API state (plain text): " + String(API_STATE_FILENAME));
    #endif

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

    // --- PLAIN TEXT IMPLEMENTATION (Decryption to be added later) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Reading API state (plain text): " + String(API_STATE_FILENAME));
    #endif

    if (!SD_MMC.exists(API_STATE_FILENAME)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] API state file does not exist.");
        #endif
        return false; // File not found is not an error for reading, just means no previous state
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
    
    File file = SD_MMC.open(fullPath.c_str(), FILE_WRITE);
    if (!file) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open text file for writing: " + fullPath);
        #endif
        return false;
    }
    size_t bytesWritten = file.print(data);
    file.close();

    if (bytesWritten != data.length()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Error: Not all text data written to file: " + fullPath);
        #endif

        return false;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Text data written to: " + fullPath); 
    #endif
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
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("[SDManager] Binary data written to: " + fullPath); 
    #endif
    return true;
}

bool SDManager::savePendingTextData(const String& subDir, const String& filename, const String& data) {
    if (!_sdAvailable) return false;
    if (!ensureDirectoryExists((String(PENDING_DATA_DIR) + "/" + subDir).c_str())) { 
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to ensure pending subdirectory exists: " + String(PENDING_DATA_DIR) + "/" + subDir);
        #endif
        return false;
    }
    String path = String(PENDING_DATA_DIR) + "/" + subDir + "/" + filename;
    return writeTextFile(path, data);
}

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

    // Check if destination directory exists, attempt to create if not.
    // This requires parsing the directory from destPath.
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


    // For SD_MMC, rename is the way to move files within the same filesystem.
    if (SD_MMC.rename(srcPath.c_str(), destPath.c_str())) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] File moved successfully from " + srcPath + " to " + destPath);
        #endif
        return true;
    } else {
        // If rename fails (e.g. across different logical drives, though not an issue here, or other FS errors)
        // Fallback to copy-then-delete might be an option, but it's more complex and resource-intensive.
        // For now, we'll rely on rename.
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] moveFile: Failed to rename/move file from " + srcPath + " to " + destPath);
        #endif
        return false;
    }
}

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

void SDManager::listDir(const char * dirname, uint8_t levels){
    if (!_sdAvailable) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] SD Card not available to list directory.");
        #endif
        return;
    }
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SDManager] Listing directory: %s\n", dirname);
    #endif

    File root = SD_MMC.open(dirname);
    if(!root){
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to open directory");
        #endif
        return;
    }
    if(!root.isDirectory()){
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Not a directory");
        #endif
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.print("  DIR : ");
                Serial.println(file.name());
            #endif
            if(levels){
                listDir(file.path(), levels -1); // Use file.path() for full path
            }
        } else {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.print("  FILE: ");
                Serial.print(file.name());
                Serial.print("  SIZE: ");
                Serial.println(file.size());
            #endif
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

bool SDManager::deleteFile(const char* path) {
    if (!_sdAvailable) return false;
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SDManager] Deleting file: %s\n", path);
    #endif
    if (SD_MMC.remove(path)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] File deleted successfully.");
        #endif
        return true;
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager] Failed to delete file.");
        #endif
        return false;
    }
}

// Helper function to read a text file from SD into a String
static String readFileToString(const char* path) {
    if (!SD_MMC.exists(path)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadFile] File does not exist: %s\n", path);
        #endif
        return "";
    }
    File file = SD_MMC.open(path);
    if (!file || file.isDirectory()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadFile] Failed to open file or it's a directory: %s\n", path);
        #endif
        if (file) file.close();
        return "";
    }
    String content = file.readString();
    file.close();
    return content;
}

// Helper function to read a binary file from SD into a buffer
// Caller is responsible for freeing the buffer.
static uint8_t* readBinaryFileToBuffer(const char* path, size_t& fileSize) {
    fileSize = 0;
    if (!SD_MMC.exists(path)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadBin] File does not exist: %s\n", path);
        #endif
        return nullptr;
    }
    File file = SD_MMC.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadBin] Failed to open file or it's a directory: %s\n", path);
        #endif
        if (file) file.close();
        return nullptr;
    }
    
    fileSize = file.size();
    if (fileSize == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadBin] File is empty: %s\n", path);
        #endif
        file.close();
        return nullptr;
    }

    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadBin] Failed to allocate %u bytes for file: %s\n", fileSize, path);
        #endif
        file.close();
        return nullptr;
    }

    size_t bytesRead = file.read(buffer, fileSize);
    file.close();

    if (bytesRead != fileSize) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ReadBin] Failed to read full file content (%u/%u bytes): %s\n", bytesRead, fileSize, path);
        #endif
        free(buffer);
        return nullptr;
    }
    return buffer;
}


bool SDManager::processPendingApiCalls(API& api_comm, TimeManager& timeMgr, Config& cfg, float internalTempForLog, float internalHumForLog) {
    if (!_sdAvailable || !api_comm.isActivated() || WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            if (!_sdAvailable) Serial.println(F("[SDManager_Pending] SD not available."));
            else if (!api_comm.isActivated()) Serial.println(F("[SDManager_Pending] API not activated."));
            else if (WiFi.status() != WL_CONNECTED) Serial.println(F("[SDManager_Pending] WiFi not connected."));
            Serial.println(F("[SDManager_Pending] Skipping processing of pending API calls."));
        #endif
        return false;
    }

    bool workDone = false;
    String logUrl = api_comm.getBaseApiUrl() + cfg.apiLogPath; // For logging send attempts

    // --- 1. Process Pending Ambient Data ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager_Pending] Checking for pending ambient data..."));
    #endif
    File ambientPendingDir = SD_MMC.open(AMBIENT_PENDING_DIR);
    if (ambientPendingDir && ambientPendingDir.isDirectory()) {
        File entry = ambientPendingDir.openNextFile();
        while (entry) {
            if (!entry.isDirectory() && String(entry.name()).endsWith("_env.json")) {
                workDone = true;
                String filePath = String(entry.path());
                String fileNameOnly = String(entry.name());
                #ifdef ENABLE_DEBUG_SERIAL
                    Serial.println("[SDManager_Pending] Processing ambient file: " + filePath);
                #endif
                entry.close(); // Close file before reopening or operating on it by path

                String jsonData = readFileToString(filePath.c_str());
                if (!jsonData.isEmpty()) {
                    // Parse JSON to get individual values needed by IOEnvironmentData
                    JsonDocument doc;
                    DeserializationError error = deserializeJson(doc, jsonData);
                    if (!error) {
                        float light = doc["light"] | NAN; // Use NAN or suitable default if missing
                        float temp = doc["temperature"] | NAN;
                        float hum = doc["humidity"] | NAN;

                        // Attempt to send (using a simplified direct call to IOEnvironmentData for this example)
                        // Note: IOEnvironmentData doesn't handle token refresh itself.
                        // A more robust solution might involve a method in API class that takes the pre-formed JSON
                        // and handles auth/refresh, or sendEnvironmentDataToServer_Env could be refactored.
                        // For now, using IOEnvironmentData directly with current access token.
                        String targetApiUrl = api_comm.getBaseApiUrl() + cfg.apiAmbientDataPath;
                        int httpCode = EnvironmentDataJSON::IOEnvironmentData(targetApiUrl, api_comm.getAccessToken(), light, temp, hum);

                        if (httpCode == 200 || httpCode == 204) {
                            #ifdef ENABLE_DEBUG_SERIAL
                                Serial.println("[SDManager_Pending] Successfully sent pending ambient data: " + fileNameOnly);
                            #endif
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::INFO, "Sent pending ambient data: " + fileNameOnly, internalTempForLog, internalHumForLog);
                            String archivePath = String(ARCHIVE_ENVIRONMENTAL_DIR) + "/" + fileNameOnly;
                            if (moveFile(filePath, archivePath)) {
                                #ifdef ENABLE_DEBUG_SERIAL
                                    Serial.println("[SDManager_Pending] Moved ambient data to archive: " + archivePath);
                                #endif
                            } else {
                                #ifdef ENABLE_DEBUG_SERIAL
                                    Serial.println("[SDManager_Pending] Failed to move ambient data to archive. Deleting from pending.");
                                #endif
                                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Failed to move sent ambient data to archive: " + fileNameOnly + ". Deleting.", internalTempForLog, internalHumForLog);
                                deleteFile(filePath.c_str()); // Delete if move failed to prevent reprocessing
                            }
                        } else if (httpCode == 401) {
                            #ifdef ENABLE_DEBUG_SERIAL
                                Serial.println("[SDManager_Pending] Auth (401) error sending pending ambient data: " + fileNameOnly + ". Will retry later if token refreshes.");
                            #endif
                            // Let the main loop handle token refresh. File remains in pending.
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Auth error sending pending ambient: " + fileNameOnly + ". HTTP: " + String(httpCode), internalTempForLog, internalHumForLog);
                        }else {
                            #ifdef ENABLE_DEBUG_SERIAL
                                Serial.println("[SDManager_Pending] Failed to send pending ambient data: " + fileNameOnly + ". HTTP Code: " + String(httpCode) + ". Will retry later.");
                            #endif
                            ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed send pending ambient: " + fileNameOnly + ". HTTP: " + String(httpCode), internalTempForLog, internalHumForLog);
                        }
                    } else {
                        #ifdef ENABLE_DEBUG_SERIAL
                            Serial.println("[SDManager_Pending] Failed to parse JSON from pending ambient file: " + filePath + ". Error: " + error.c_str());
                        #endif
                        ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Failed to parse pending ambient JSON: " + fileNameOnly, internalTempForLog, internalHumForLog);
                        // Consider moving to a "corrupted_pending" directory or deleting. For now, leaves it.
                    }
                } else {
                     #ifdef ENABLE_DEBUG_SERIAL
                        Serial.println("[SDManager_Pending] Pending ambient file is empty or failed to read: " + filePath);
                    #endif
                    ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Empty/unreadable pending ambient file: " + fileNameOnly, internalTempForLog, internalHumForLog);
                }
                entry = ambientPendingDir.openNextFile(); // Process next file
            } else {
                 if(entry.isDirectory()) { // Skip directories
                    #ifdef ENABLE_DEBUG_SERIAL
                        Serial.println("[SDManager_Pending] Skipping directory in ambient_pending: " + String(entry.name()));
                    #endif
                 }
                entry.close(); // Close the current entry
                entry = ambientPendingDir.openNextFile(); // Get next
            }
        }
        if(entry) entry.close(); // Close last opened entry if loop exited due to entry being null
        ambientPendingDir.close();
    } else {
         #ifdef ENABLE_DEBUG_SERIAL
            if(!ambientPendingDir) Serial.println(F("[SDManager_Pending] Could not open ambient pending directory."));
        #endif
    }

    // --- PARTE 2: PROCESAR DATOS DE CAPTURA PENDIENTES (Lógica Mejorada) ---
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager_Pending] Checking for pending capture data..."));
    #endif
    File capturePendingDir = SD_MMC.open(CAPTURE_PENDING_DIR);
    if (!capturePendingDir) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager_Pending] Could not open capture pending directory."));
        #endif
        return false;
    }
    
    std::vector<String> thermalJsonFiles;
    File entry = capturePendingDir.openNextFile();
    while(entry){
        if(!entry.isDirectory() && String(entry.name()).endsWith("_thermal.json")){
            thermalJsonFiles.push_back(String(entry.path()));
        }
        entry.close(); // Siempre cerrar el archivo después de usarlo
        entry = capturePendingDir.openNextFile();
    }
    capturePendingDir.close();

    for (const String& thermalJsonPath : thermalJsonFiles) {
        workDone = true;
        String baseName = thermalJsonPath.substring(thermalJsonPath.lastIndexOf('/') + 1);
        baseName = baseName.substring(0, baseName.indexOf("_thermal.json"));
        String visualJpgPath = String(CAPTURE_PENDING_DIR) + "/" + baseName + "_visual.jpg";
        String thermalFileNameOnly = baseName + "_thermal.json";
        
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Pending] Processing thermal file: " + thermalFileNameOnly);
        #endif

        // Diferenciar entre un par completo y un archivo térmico huérfano
        if (SD_MMC.exists(visualJpgPath.c_str())) {
            // --- CASO 1: Existe un par completo (Térmica + Visual) ---
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SDManager_Pending]   -> Visual counterpart found. Processing as a pair.");
            #endif
            
            String visualFileNameOnly = baseName + "_visual.jpg";
            String thermalJsonContent = readFileToString(thermalJsonPath.c_str());
            size_t jpegLength = 0;
            uint8_t* jpegImage = readBinaryFileToBuffer(visualJpgPath.c_str(), jpegLength);

            // VERIFICACIÓN: Si alguno de los archivos está vacío o es ilegible, se borra el par.
            if (thermalJsonContent.isEmpty() || !jpegImage || jpegLength == 0) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Unreadable/empty pending capture pair: " + baseName + ". Deleting.", internalTempForLog, internalHumForLog);
                deleteFile(thermalJsonPath.c_str());
                deleteFile(visualJpgPath.c_str());
                if(jpegImage) free(jpegImage); // Liberar si el buffer se asignó pero el archivo estaba vacío
                continue; // Pasar al siguiente archivo
            }

            float* thermalDataArray = parseThermalJson(thermalJsonContent);
            // VERIFICACIÓN: Si el JSON está corrupto, se borra el par.
            if (!thermalDataArray) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Corrupted pending JSON in pair: " + baseName + ". Deleting.", internalTempForLog, internalHumForLog);
                deleteFile(thermalJsonPath.c_str());
                deleteFile(visualJpgPath.c_str());
                free(jpegImage);
                continue; // Pasar al siguiente archivo
            }

            int httpCode = MultipartDataSender::IOThermalAndImageData(api_comm.getBaseApiUrl() + cfg.apiCaptureDataPath, api_comm.getAccessToken(), thermalDataArray, jpegImage, jpegLength);

            if (httpCode >= 200 && httpCode < 300) {
                archiveFile(thermalJsonPath, String(ARCHIVE_CAPTURES_DIR) + "/" + thermalFileNameOnly);
                archiveFile(visualJpgPath, String(ARCHIVE_CAPTURES_DIR) + "/" + visualFileNameOnly);
            } else {
                 ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed to send pending pair " + baseName + ", HTTP: " + String(httpCode), internalTempForLog, internalHumForLog);
            }

            free(thermalDataArray);
            free(jpegImage);

        } else {
            // --- CASO 2: Archivo térmico huérfano (sin contraparte visual) ---
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("[SDManager_Pending]   -> No visual counterpart. Processing as thermal-only.");
            #endif

            String thermalJsonContent = readFileToString(thermalJsonPath.c_str());
            // VERIFICACIÓN: Si el archivo está vacío o es ilegible, se borra.
            if (thermalJsonContent.isEmpty()) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Unreadable pending thermal-only file: " + thermalFileNameOnly + ". Deleting.", internalTempForLog, internalHumForLog);
                deleteFile(thermalJsonPath.c_str());
                continue; // Pasar al siguiente archivo
            }

            float* thermalDataArray = parseThermalJson(thermalJsonContent);
            // VERIFICACIÓN: Si el JSON está corrupto, se borra.
            if (!thermalDataArray) {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::ERROR, "Corrupted pending thermal-only JSON: " + thermalFileNameOnly + ". Deleting.", internalTempForLog, internalHumForLog);
                deleteFile(thermalJsonPath.c_str());
                continue; // Pasar al siguiente archivo
            }
            
            int httpCode = MultipartDataSender::IOThermalAndImageData(api_comm.getBaseApiUrl() + cfg.apiCaptureDataPath, api_comm.getAccessToken(), thermalDataArray, nullptr, 0);

            if (httpCode >= 200 && httpCode < 300) {
                archiveFile(thermalJsonPath, String(ARCHIVE_CAPTURES_DIR) + "/" + thermalFileNameOnly);
            } else {
                ErrorLogger::logToSdOnly(*this, timeMgr, LogLevel::WARNING, "Failed to send pending thermal-only " + baseName + ", HTTP: " + String(httpCode), internalTempForLog, internalHumForLog);
            }
            
            free(thermalDataArray);
        }
    }
    
    #ifdef ENABLE_DEBUG_SERIAL
        if(workDone) Serial.println(F("[SDManager_Pending] Finished processing pending API calls."));
        else Serial.println(F("[SDManager_Pending] No pending files found to process."));
    #endif
    return workDone;
}

// Helper to parse YYYYMMDD_HHMMSS from filename start
time_t SDManager::_parseTimestampFromFilename(const String& filename, TimeManager& timeMgr) {
    struct tm t{};
    
    if (filename.length() < 8) {
        return 0; 
    }

    if (sscanf(filename.c_str(), "%4d%2d%2d", &t.tm_year, &t.tm_mon, &t.tm_mday) == 3) {
        t.tm_year -= 1900;
        t.tm_mon -= 1;     
        t.tm_hour = 0; 
        t.tm_min = 0; 
        t.tm_sec = 0;
        t.tm_isdst = -1;
        
        return mktime(&t);
    }
    
    return 0; 
}

uint64_t SDManager::_manageDirectory(const String& dirPath, TimeManager& timeMgr, int maxFileAgeDays, 
                                   uint64_t minTotalFreeBytes, uint64_t& currentTotalUsedBytes, uint64_t totalSdSizeBytes) {
    if (!_sdAvailable) return 0;

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[SDManager_Manage] Managing directory: %s (MaxAge: %d days, MinFreeGlobally: %llu bytes)\n", dirPath.c_str(), maxFileAgeDays, minTotalFreeBytes);
    #endif

    File root = SD_MMC.open(dirPath.c_str());
    if (!root || !root.isDirectory()) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_Manage] Failed to open directory: %s\n", dirPath.c_str());
        #endif
        if(root) root.close();
        return 0;
    }

    std::vector<FileInfo> files;
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String path = String(entry.path()); // Get full path
            String name = String(entry.name());
            time_t timestamp = _parseTimestampFromFilename(name, timeMgr);
            if (timestamp > 0) { // Successfully parsed timestamp
                files.push_back({path, timestamp});
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[SDManager_Manage] Could not parse timestamp from: %s\n", name.c_str());
                #endif
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();

    if (files.empty()) {
        #ifdef ENABLE_DEBUG_SERIAL
            // Serial.printf("[SDManager_Manage] No files found in %s to manage.\n", dirPath.c_str());
        #endif
        return 0;
    }
    
    // Sort files by timestamp (oldest first)
    std::sort(files.begin(), files.end());

    uint64_t bytesFreedThisRun = 0;
    time_t currentTime = timeMgr.getCurrentEpochTime(); // Get current epoch time
    if (currentTime == 0 && timeMgr.isTimeSynced()) { // Check if epoch is valid if time is supposed to be synced
         #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Manage] Warning: Current epoch time is 0, but time claims to be synced. Age calculation might be incorrect.");
        #endif
        // Potentially return or use a less aggressive strategy if current time is unreliable for age calc
    }
    
    time_t maxAgeSeconds = (time_t)maxFileAgeDays * 24 * 60 * 60;

    // Phase 1: Delete files older than maxFileAgeDays
    #ifdef ENABLE_DEBUG_SERIAL
        int filesDeletedByAge = 0;
    #endif
    // Iterate backwards to safely remove elements or use std::remove_if
    for (auto it = files.begin(); it != files.end(); /* increment handled in loop */) {
        bool removed = false;
        if (currentTime > 0 && (currentTime - it->timestamp) > maxAgeSeconds) {
            File f = SD_MMC.open(it->path.c_str());
            uint64_t fileSize = 0;
            if (f) {
                fileSize = f.size();
                f.close();
            }
            if (deleteFile(it->path.c_str())) {
                bytesFreedThisRun += fileSize;
                currentTotalUsedBytes -= fileSize; // Update global used bytes
                #ifdef ENABLE_DEBUG_SERIAL
                    filesDeletedByAge++;
                #endif
                it = files.erase(it); // Erase and get next valid iterator
                removed = true;
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[SDManager_Manage] Failed to delete (by age) old file: %s\n", it->path.c_str());
                #endif
            }
        }
        if (!removed) {
            ++it;
        }
    }
    #ifdef ENABLE_DEBUG_SERIAL
        if (filesDeletedByAge > 0) {
            Serial.printf("[SDManager_Manage] Deleted %d file(s) from %s due to age.\n", filesDeletedByAge, dirPath.c_str());
        }
    #endif

    // Phase 2: If still not enough free space globally, delete more (oldest first) from this directory
    #ifdef ENABLE_DEBUG_SERIAL
        int filesDeletedBySpace = 0;
    #endif
    // Files are already sorted oldest first
    for (auto it = files.begin(); it != files.end(); /* increment handled in loop */) {
        if ((totalSdSizeBytes - currentTotalUsedBytes) < minTotalFreeBytes) {
            File f = SD_MMC.open(it->path.c_str());
            uint64_t fileSize = 0;
            if (f) {
                fileSize = f.size();
                f.close();
            }
            if (deleteFile(it->path.c_str())) {
                bytesFreedThisRun += fileSize;
                currentTotalUsedBytes -= fileSize;
                #ifdef ENABLE_DEBUG_SERIAL
                    filesDeletedBySpace++;
                #endif
                it = files.erase(it); // Erase and get next
            } else {
                 #ifdef ENABLE_DEBUG_SERIAL
                    Serial.printf("[SDManager_Manage] Failed to delete (for space) file: %s\n", it->path.c_str());
                #endif
                ++it; // crucial: if delete fails, still advance iterator to avoid infinite loop on a stubborn file
            }
        } else {
            break; // Enough space freed or no more files to check in this directory for space
        }
    }
     #ifdef ENABLE_DEBUG_SERIAL
        if (filesDeletedBySpace > 0) {
            Serial.printf("[SDManager_Manage] Deleted %d additional file(s) from %s to free up space.\n", filesDeletedBySpace, dirPath.c_str());
        }
    #endif
    
    return bytesFreedThisRun;
}


void SDManager::manageAllStorage(TimeManager& timeMgr, int maxFileAgeDays, float minFreeSpacePercentage) {
    if (!_sdAvailable) return;

    // --- OPTIMIZATION: Only run the full, slow check if disk usage is high ---
    uint64_t totalBytes = SD_MMC.totalBytes();
    if (totalBytes == 0) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println(F("[SDManager_ManageAll] SD Card total size is 0. Cannot manage storage."));
        #endif
        return;
    }
    
    uint64_t usedBytes = SD_MMC.usedBytes();
    float usagePercent = ((float)usedBytes / totalBytes) * 100.0f;

    // Set a threshold to trigger the cleanup. 90% is a reasonable default.
    const float CLEANUP_TRIGGER_PERCENTAGE = 90.0f; 

    if (usagePercent < CLEANUP_TRIGGER_PERCENTAGE) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.printf("[SDManager_ManageAll] Disk usage is at %.2f%% (below %.0f%% threshold). Skipping full storage management scan.\n", usagePercent, CLEANUP_TRIGGER_PERCENTAGE);
        #endif
        return; // Exit early, no need to perform the expensive scan.
    }
    // --- END OPTIMIZATION ---


    // If we reach here, it means disk usage is high and we need to perform the cleanup.
    uint64_t minFreeBytesAbsolute = (uint64_t)(totalBytes * (minFreeSpacePercentage / 100.0f));

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println(F("[SDManager_ManageAll] --- Starting Storage Management ---"));
        Serial.printf("[SDManager_ManageAll] Total: %llu MB, Used: %llu MB, Free: %llu MB\n", totalBytes / (1024*1024), usedBytes / (1024*1024), (totalBytes - usedBytes) / (1024*1024) );
        Serial.printf("[SDManager_ManageAll] Policy: MaxAge %d days, MinFree %.2f%% (%llu bytes)\n", maxFileAgeDays, minFreeSpacePercentage, minFreeBytesAbsolute);
    #endif

    // The currentTotalUsedBytes is passed by reference and updated by _manageDirectory.
    _manageDirectory(LOG_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);
    _manageDirectory(ARCHIVE_ENVIRONMENTAL_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);
    _manageDirectory(ARCHIVE_CAPTURES_DIR, timeMgr, maxFileAgeDays, minFreeBytesAbsolute, usedBytes, totalBytes);

    #ifdef ENABLE_DEBUG_SERIAL
        uint64_t finalUsedBytes = SD_MMC.usedBytes();
        Serial.printf("[SDManager_ManageAll] Storage Management Complete. Final Used: %llu MB, Final Free: %llu MB\n", finalUsedBytes / (1024*1024), (totalBytes - finalUsedBytes) / (1024*1024));
        Serial.println(F("[SDManager_ManageAll] ------------------------------------"));
    #endif
}

float* SDManager::parseThermalJson(const String& jsonContent) {
    JsonDocument thermalDoc;
    DeserializationError error = deserializeJson(thermalDoc, jsonContent);
    if (error) return nullptr;

    JsonArray temps = thermalDoc["temperatures"].as<JsonArray>();
    if (temps.isNull() || temps.size() != MultipartDataSender::THERMAL_PIXELS) {
        return nullptr;
    }

    float* thermalDataArray = (float*)malloc(MultipartDataSender::THERMAL_PIXELS * sizeof(float));
    if (!thermalDataArray) return nullptr;

    for (int i = 0; i < MultipartDataSender::THERMAL_PIXELS; ++i) {
        thermalDataArray[i] = temps[i].as<float>();
    }
    return thermalDataArray;
}

void SDManager::archiveFile(const String& srcPath, const String& destPath) {
    if (moveFile(srcPath, destPath)) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Pending] Moved to archive: " + destPath);
        #endif
    } else {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[SDManager_Pending] Failed to move to archive. Deleting from pending: " + srcPath);
        #endif
        deleteFile(srcPath.c_str());
    }
}