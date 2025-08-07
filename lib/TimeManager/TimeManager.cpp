// lib/TimeManager/TimeManager.cpp
#include "TimeManager.h"
#include <WiFi.h> // To check WiFi status before attempting NTP sync

TimeManager::TimeManager() : 
    _timeSynchronized(false),
    _ntpServer1(DEFAULT_NTP_SERVER_1),
    _ntpServer2(DEFAULT_NTP_SERVER_2),
    _gmtOffset_sec(0),
    _daylightOffset_sec(0) {
    // Constructor
}

void TimeManager::begin(const String& ntpServer1, const String& ntpServer2, long gmtOffset_sec, int daylightOffset_sec) {
    _ntpServer1 = ntpServer1;
    _ntpServer2 = ntpServer2;
    _gmtOffset_sec = gmtOffset_sec;
    _daylightOffset_sec = daylightOffset_sec;

    // configTime is an ESP-IDF function that configures NTP.
    // It starts the SNTP service.
    // Parameters: gmtOffset_sec, daylightOffset_sec, server1, server2, (optional server3)
    configTime(_gmtOffset_sec, _daylightOffset_sec, _ntpServer1.c_str(), _ntpServer2.c_str());
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.printf("[TimeManager] Initialized with NTP Servers: %s, %s. GMT Offset: %ld, DST Offset: %d\n",
                      _ntpServer1.c_str(), _ntpServer2.c_str(), _gmtOffset_sec, _daylightOffset_sec);
    #endif
    // Initial sync attempt can be called from main setup after WiFi connection
}

bool TimeManager::syncNtpTime() {
    if (WiFi.status() != WL_CONNECTED) {
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[TimeManager] NTP Sync failed: WiFi not connected.");
        #endif
        _timeSynchronized = false; // Mark as not synced if WiFi is down
        return false;
    }

    #ifdef ENABLE_DEBUG_SERIAL
        Serial.print("[TimeManager] Attempting NTP time synchronization");
    #endif

    // configTime should have already been called in begin().
    // Here we just check if time is available.
    // The getLocalTime function will block until time is obtained or timeout (default is about 15 seconds per server).
    
    struct tm timeinfo;
    int retries = 0;
    while(!getLocalTime(&timeinfo, (NTP_SYNC_MAX_RETRIES * 15000) + 5000)) { // Max wait slightly more than all retries on default timeouts
                                                                          // Or manage retries manually if getLocalTime is non-blocking enough
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.print(".");
        #endif
        delay(NTP_SYNC_RETRY_DELAY_MS);
        retries++;
        if (retries >= NTP_SYNC_MAX_RETRIES) {
            #ifdef ENABLE_DEBUG_SERIAL
                Serial.println("\n[TimeManager] Failed to obtain NTP time after multiple retries.");
            #endif
            // _timeSynchronized remains false or is set to false explicitly
            // No need to call configTime again here unless changing servers
            _timeSynchronized = false; 
            return false;
        }
        // If WiFi disconnects during attempts, exit
        if (WiFi.status() != WL_CONNECTED) {
            #ifdef ENABLE_DEBUG_SERIAL
                 Serial.println("\n[TimeManager] NTP Sync aborted: WiFi disconnected during sync attempt.");
            #endif
            _timeSynchronized = false;
            return false;
        }
    }
    
    #ifdef ENABLE_DEBUG_SERIAL
        Serial.println("\n[TimeManager] NTP Time synchronized successfully.");
        Serial.printf("[TimeManager] Current time: %s", asctime(&timeinfo)); // asctime adds a newline
    #endif
    _timeSynchronized = true;
    return true;
}

String TimeManager::getCurrentTimestampString(bool forFileNames) {
    if (!_timeSynchronized) {
        // Fallback: Use millis() for a relative timestamp if NTP is not synced
        // This indicates that the timestamp is not from a real-time clock.
        unsigned long ms = millis();
        unsigned long s = ms / 1000;
        unsigned long m = s / 60;
        unsigned long h = m / 60;
        // Format: UPTIME_HHhMMmSSs (Uptime based)
        char buf[30];
        sprintf(buf, "UPTIME_%02luh%02lum%02lus", h % 24, m % 60, s % 60);
        return String(buf);
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) { // Should return quickly if already synced
        #ifdef ENABLE_DEBUG_SERIAL
            Serial.println("[TimeManager] Failed to get local time structure even after sync.");
        #endif
        return "TIME_STRUCT_ERROR";
    }

    char buffer[30];
    if (forFileNames) {
        // Format: YYYYMMDD_HHMMSS
        strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &timeinfo);
    } else {
        // Format: YYYY-MM-DD HH:MM:SS
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }
    return String(buffer);
}

time_t TimeManager::getCurrentEpochTime() {
    if (!_timeSynchronized) {
        return 0; // Return 0 or some indicator of unsynced time
    }
    time_t now;
    time(&now); // Gets current calendar time as time_t object (seconds since epoch)
    // 'now' will be based on the ESP32's internal RTC, which should have been set by configTime/NTP.
    return now;
}

bool TimeManager::isTimeSynced() const {
    return _timeSynchronized;
}