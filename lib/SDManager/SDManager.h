// lib/SDManager/SDManager.h
#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include "FS.h"      // Filesystem base class
#include "SD_MMC.h"  // SD Card library for ESP32 (using SDMMC peripheral)
#include <vector>   // For managing lists of files
#include <algorithm> // Required for std::sort in manageLogStorage 

#include "API.h" // Required for api_comm
#include "TimeManager.h" // Required for timeMgr
#include "ConfigManager.h" // Required for cfg (Config struct)
#include "ErrorLogger.h"   // For logging results of send attempts
#include "EnvironmentDataJSON.h" // For resending ambient data
#include "MultipartDataSender.h" // For resending capture data

// Define log levels that can be used when logging messages
enum class LogLevel {
    INFO,
    WARNING,
    ERROR
};

// Standard directory paths and filenames
#define LOG_DIR "/logs"
#define SECURE_DATA_DIR "/secure_data" // For API state, etc.
#define PENDING_DATA_DIR "/data_pending"
#define AMBIENT_PENDING_DIR PENDING_DATA_DIR "/ambient"
#define CAPTURE_PENDING_DIR PENDING_DATA_DIR "/capture"

// Directories for long-term archival
#define ARCHIVE_DIR "/archive"
#define ARCHIVE_ENVIRONMENTAL_DIR ARCHIVE_DIR "/environmental"
#define ARCHIVE_CAPTURES_DIR ARCHIVE_DIR "/captures"

#define API_STATE_FILENAME SECURE_DATA_DIR "/api_state.json" // Storing API token, activation, etc.

class API; // Forward declaration for processPendingApiCalls

class SDManager {
public:
    /**
     * @brief Constructor for SDManager.
     */
    SDManager();

    /**
     * @brief Initializes the SD card using the SD_MMC peripheral.
     * Attempts to mount the SD card in 4-bit SD_MMC mode.
     * Creates necessary base directories if they don't exist.
     * @return True if SD card is initialized and available, false otherwise.
     */
    bool begin();

    /**
     * @brief Checks if the SD card is available and was initialized successfully.
     * @return True if SD card is ready, false otherwise.
     */
    bool isSDAvailable() const;

    /**
     * @brief Writes a formatted log message to a daily log file on the SD card.
     * Log files are named /logs/LOG_YYYYMMDD.txt.
     * @param timestamp String representing the current date and time.
     * @param level The severity level of the log message (INFO, WARNING, ERROR).
     * @param message The main log message content.
     * @param internalTemp Optional internal temperature reading to include in the log.
     * @param internalHum Optional internal humidity reading to include in the log.
     * @return True if the log message was successfully written, false otherwise.
     */
    bool logToFile(const String& timestamp, LogLevel level, const String& message, float internalTemp = NAN, float internalHum = NAN);

    /**
     * @brief Saves application state (e.g., API tokens, activation status) as a JSON string to a file on the SD card.
     * Currently saves as plain text. Encryption can be added here later.
     * @param stateJson The JSON string representing the application state to save.
     * @return True if the state was successfully saved, false otherwise.
     */
    bool saveApiState(const String& stateJson);

    /**
     * @brief Reads application state from a file on the SD card into a JSON string.
     * Currently reads plain text. Decryption would be added here if saveApiState encrypts.
     * @param stateJsonOut Reference to a String where the read JSON state will be stored.
     * @return True if the state was successfully read, false if the file doesn't exist or an error occurs.
     */
    bool readApiState(String& stateJsonOut);

    /**
     * @brief Saves generic text data (e.g., JSON payload for a pending ambient data API call) to a specified path on the SD card.
     * @param subDir The subdirectory within PENDING_DATA_DIR (e.g., "ambient", "capture").
     * @param filename The name for the file (e.g., based on a timestamp).
     * @param data The String data to save.
     * @return True if data was successfully saved, false otherwise.
     */
    bool savePendingTextData(const String& subDir, const String& filename, const String& data);

    /**
     * @brief Saves generic binary data (e.g., a JPEG image for a pending capture data API call) to a specified path on the SD card.
     * @param subDir The subdirectory within PENDING_DATA_DIR (e.g., "ambient", "capture").
     * @param filename The name for the file (e.g., based on a timestamp).
     * @param data Pointer to the binary data buffer.
     * @param length The length of the binary data in bytes.
     * @return True if data was successfully saved, false otherwise.
     */
    bool savePendingBinaryData(const String& subDir, const String& filename, const uint8_t* data, size_t length);

     /**
     * @brief Writes text data to a file at the specified full path.
     * Creates the file if it does not exist, overwrites it if it does.
     * @param fullPath The complete path including filename (e.g., "/archive/environmental/data.json").
     * @param data The String data to save.
     * @return True if data was successfully written, false otherwise.
     */
    bool writeTextFile(const String& fullPath, const String& data);

    /**
     * @brief Writes binary data to a file at the specified full path.
     * Creates the file if it does not exist, overwrites it if it does.
     * @param fullPath The complete path including filename (e.g., "/archive/captures/image.jpg").
     * @param data Pointer to the binary data buffer.
     * @param length The length of the binary data in bytes.
     * @return True if data was successfully written, false otherwise.
     */
    bool writeBinaryFile(const String& fullPath, const uint8_t* data, size_t length);

    /**
     * @brief Moves a file from a source path to a destination path.
     * Currently, this will be implemented as copy then delete, as SD_MMC might not have a direct 'move' or 'rename'.
     * @param srcPath The full source path of the file.
     * @param destPath The full destination path for the file.
     * @return True if the file was successfully moved, false otherwise.
     */
    bool moveFile(const String& srcPath, const String& destPath);

    /**
     * @brief Lists files and directories within a given path.
     * Useful for debugging and can be expanded for file management.
     * @param dirname The path of the directory to list.
     * @param levels The number of directory levels to recurse into.
     */
    void listDir(const char* dirname, uint8_t levels);
    
    /**
     * @brief Deletes a file at the specified path.
     * @param path The full path to the file to be deleted.
     * @return True if deletion was successful, false otherwise.
     */
    bool deleteFile(const char* path);

    /**
     * @brief Processes pending API calls stored on the SD card.
     * Iterates through files in AMBIENT_PENDING_DIR and CAPTURE_PENDING_DIR.
     * Attempts to send the data using the API object.
     * If successful, moves the file(s) to the corresponding archive directory.
     * @param api_comm Reference to the API communication object.
     * @param timeMgr Reference to the TimeManager object (for logging).
     * @param cfg Reference to the Config object (for API paths, etc.).
     * @param internalTempForLog Internal temperature for logging send attempts.
     * @param internalHumForLog Internal humidity for logging send attempts.
     * @return True if any pending data was processed (successfully or not), false if no pending data found.
     */
    bool processPendingApiCalls(API& api_comm, TimeManager& timeMgr, Config& cfg, float internalTempForLog = NAN, float internalHumForLog = NAN);

    /**
     * @brief Manages storage for logs and archived data based on age and disk space.
     * Deletes files older than a specified maximum age or if free disk space falls below a minimum percentage.
     * Applied to LOG_DIR, ARCHIVE_ENVIRONMENTAL_DIR, and ARCHIVE_CAPTURES_DIR.
     * Oldest files are prioritized for deletion when clearing space.
     * @param timeMgr Reference to TimeManager to get the current date for age calculations.
     * @param maxFileAgeDays Maximum age of files to keep (e.g., 365 for one year).
     * @param minFreeSpacePercentage Minimum percentage of total disk space to keep free (e.g., 5.0 for 5%).
     */
    void manageAllStorage(TimeManager& timeMgr, int maxFileAgeDays = 365, float minFreeSpacePercentage = 5.0f);

    /**
     * @brief Gets the SD card usage information.
     * @param[out] outUsedBytes Reference to store the used bytes on the SD card.
     * @param[out] outTotalBytes Reference to store the total bytes on the SD card.
     * @return Usage percentage (0.0 to 100.0). Returns -1.0f if SD is not available or totalBytes is 0.
     */
    float getUsageInfo(uint64_t& outUsedBytes, uint64_t& outTotalBytes);

private:
    bool _sdAvailable; // Flag to indicate if SD card is initialized and ready

    struct FileInfo {
        String path;
        time_t timestamp; // Epoch time for easy sorting and age calculation

        bool operator<(const FileInfo& other) const {
            return timestamp < other.timestamp; // Sort by oldest first
        }
    };

    /**
     * @brief Helper function to create a directory if it doesn't already exist.
     * @param path The path of the directory to create.
     * @return True if directory exists or was created successfully, false otherwise.
     */
    bool ensureDirectoryExists(const char* path);

    /**
     * @brief Converts LogLevel enum to a string representation.
     * @param level The LogLevel enum value.
     * @return String representation of the log level (e.g., "INFO").
     */
    String logLevelToString(LogLevel level);

    /**
     * @brief Internal helper to manage contents of a single directory.
     * @param dirPath Path to the directory to manage.
     * @param timeMgr Reference to TimeManager.
     * @param maxFileAgeDays Max age for files in this directory.
     * @param minTotalFreeBytes Absolute minimum bytes to keep free on the entire SD card.
     * @param currentTotalUsedBytes Current total used bytes on the SD card (to check if deletion is needed for space).
     * @param totalSdSizeBytes Total size of the SD card in bytes.
     * @return Number of bytes freed by this call for this directory.
     */
    uint64_t _manageDirectory(const String& dirPath, TimeManager& timeMgr, int maxFileAgeDays, uint64_t minTotalFreeBytes, uint64_t& currentTotalUsedBytes, uint64_t totalSdSizeBytes);
    
    /**
     * @brief Parses a filename (expected format YYYYMMDD_HHMMSS*) to extract an epoch timestamp.
     * @param filename The name of the file.
     * @param timeMgr Reference to TimeManager to help with struct tm to time_t conversion respecting timezone.
     * @return Epoch timestamp, or 0 if parsing fails.
     */
    time_t _parseTimestampFromFilename(const String& filename, TimeManager& timeMgr);

    /**
     * @brief Parses a thermal JSON string and extracts the temperature array.
     * Handles memory allocation for the float array. Caller is responsible for freeing memory.
     * @param jsonContent The string containing the thermal JSON data.
     * @return A pointer to a newly allocated float array of 768 pixels, or nullptr if parsing fails or memory allocation fails.
     */
    float* parseThermalJson(const String& jsonContent);

    /**
     * @brief Helper to move a file to the archive. If moving fails, it deletes the source file to prevent resending.
     * @param srcPath The full path of the source file in the pending directory.
     * @param destPath The full path of the destination in the archive directory.
     */
    void archiveFile(const String& srcPath, const String& destPath);
};

#endif // SD_MANAGER_H