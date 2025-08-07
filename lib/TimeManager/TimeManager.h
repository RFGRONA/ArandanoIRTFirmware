// lib/TimeManager/TimeManager.h
#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include "time.h" // Standard C time library

// Default NTP servers - can be overridden by config
#define DEFAULT_NTP_SERVER_1 "pool.ntp.org"
#define DEFAULT_NTP_SERVER_2 "time.nist.gov"

class TimeManager {
public:
    /**
     * @brief Constructor for TimeManager.
     */
    TimeManager();

    /**
     * @brief Initializes the time client with NTP servers and timezone information.
     * Call this once in setup() after WiFi is connected.
     * @param ntpServer1 Primary NTP server hostname.
     * @param ntpServer2 Secondary NTP server hostname.
     * @param gmtOffset_sec GMT offset in seconds for the local timezone.
     * @param daylightOffset_sec Daylight saving time offset in seconds (usually 0 or 3600).
     */
    void begin(const String& ntpServer1 = DEFAULT_NTP_SERVER_1, 
               const String& ntpServer2 = DEFAULT_NTP_SERVER_2, 
               long gmtOffset_sec = -5 * 3600, // 0 = Default to UTC
               int daylightOffset_sec = 0);

    /**
     * @brief Attempts to synchronize the local time with an NTP server.
     * This function can be called periodically to update/correct the time.
     * @return True if synchronization was successful, false otherwise.
     */
    bool syncNtpTime();

    /**
     * @brief Gets the current formatted timestamp string.
     * Format: "YYYY-MM-DD HH:MM:SS". If forFileNames is true, uses "YYYYMMDD_HHMMSS".
     * @param forFileNames If true, returns a format suitable for filenames (YYYYMMDD_HHMMSS).
     * Otherwise, returns a standard log format (YYYY-MM-DD HH:MM:SS).
     * @return A string with the current time, or "TIME_NOT_SYNCED" if NTP sync has not occurred.
     */
    String getCurrentTimestampString(bool forFileNames = false);

    /**
     * @brief Gets the current time as seconds since epoch (Unix time).
     * @return Epoch time as time_t. Returns 0 if time is not synced.
     */
    time_t getCurrentEpochTime();

    /**
     * @brief Checks if the time has been successfully synchronized with an NTP server at least once.
     * @return True if time is synchronized, false otherwise.
     */
    bool isTimeSynced() const;

private:
    bool _timeSynchronized; // Flag to indicate if NTP sync has been successful at least once
    String _ntpServer1;
    String _ntpServer2;
    long _gmtOffset_sec;
    int _daylightOffset_sec;

    // Max retries for NTP sync in one attempt
    static const int NTP_SYNC_MAX_RETRIES = 5;
    // Delay between NTP sync retries (ms)
    static const unsigned long NTP_SYNC_RETRY_DELAY_MS = 1000; 
};

#endif // TIME_MANAGER_H