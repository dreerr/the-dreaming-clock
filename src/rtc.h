#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <time.h>

// RTC Module (DS1307)
extern RTC_DS1307 rtc;
extern bool rtcInitialized;
extern bool usingInternalTime;
extern bool ntpSynced;

// I2C Pins for ESP32-C3
#define I2C_SDA 4
#define I2C_SCL 5

void setupRTC();
void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                int year);
DateTime getCurrentTime();
bool tryNTPSync();

// Global variable definitions
RTC_DS1307 rtc;
bool rtcInitialized = false;
bool usingInternalTime = false;
bool ntpSynced = false;

// NTP Configuration
static const char *ntpServer1 = "pool.ntp.org";
static const char *ntpServer2 = "time.google.com";
static const long gmtOffset_sec = 3600;     // UTC+1 (CET)
static const int daylightOffset_sec = 3600; // +1h for CEST

// Internal time tracking (fallback when RTC not connected)
static time_t internalTimeOffset = 0;
static unsigned long internalTimeSetMillis = 0;

// Try to sync time from NTP server
bool tryNTPSync() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("  NTP: No WiFi connection, skipping NTP sync");
    return false;
  }

  Serial.println("  NTP: Attempting to sync time from NTP server...");

  // Configure SNTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

  // Wait for time to be set (max 10 seconds)
  int retries = 0;
  const int maxRetries = 20;
  struct tm timeinfo;

  while (retries < maxRetries) {
    if (getLocalTime(&timeinfo, 500)) {
      // Check if we got a valid time (year > 2020)
      if (timeinfo.tm_year > 120) { // tm_year is years since 1900
        Serial.printf("  NTP: Time synced successfully!\n");
        Serial.printf("  NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                      timeinfo.tm_sec);
        ntpSynced = true;
        return true;
      }
    }
    retries++;
    delay(500);
  }

  Serial.println("  NTP: Failed to sync time (timeout)");
  return false;
}

void setupRTC() {
  Serial.println("=== RTC Setup ===");
  Serial.printf("  Module: DS1307\n");
  Serial.printf("  I2C Pins: SDA=GPIO%d, SCL=GPIO%d\n", I2C_SDA, I2C_SCL);

  // Initialize I2C with custom pins for ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    Serial.println("  WARNING: RTC not found! Using internal time.");
    rtcInitialized = false;
    usingInternalTime = true;

    // Try to sync time from NTP server
    if (tryNTPSync()) {
      timeWasSet = true;
      Serial.println("  -> Using NTP-synced time");
    } else {
      // Set internal time to a default (Jan 1, 2025, 00:00:00)
      struct tm defaultTime = {0};
      defaultTime.tm_year = 2025 - 1900; // Years since 1900
      defaultTime.tm_mon = 0;            // January (0-11)
      defaultTime.tm_mday = 1;
      defaultTime.tm_hour = 0;
      defaultTime.tm_min = 0;
      defaultTime.tm_sec = 0;
      internalTimeOffset = mktime(&defaultTime);
      internalTimeSetMillis = millis();

      timeWasSet = false;
      Serial.println("  -> Time will be lost on power cycle");
      Serial.println("  -> Set time via web interface: /settings");
    }
    Serial.println("=================\n");
    return;
  }

  rtcInitialized = true;
  usingInternalTime = false;
  Serial.println("  RTC connected successfully");

  // Check if RTC is running
  if (!rtc.isrunning()) {
    Serial.println("  WARNING: RTC has no valid time set");
    Serial.println("  -> Set time via web interface: /settings");
    timeWasSet = false;
  } else {
    // RTC has valid time
    timeWasSet = true;
    DateTime now = rtc.now();
    Serial.printf("  Current time: %02d:%02d:%02d\n", now.hour(), now.minute(),
                  now.second());
    Serial.printf("  Current date: %04d-%02d-%02d\n", now.year(), now.month(),
                  now.day());
  }
  Serial.println("=================\n");
}

// Get current time from RTC or internal fallback
DateTime getCurrentTime() {
  if (rtcInitialized) {
    return rtc.now();
  }

  // If NTP synced, use system time (maintained by ESP32)
  if (ntpSynced) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
      return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
                      timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
                      timeinfo.tm_sec);
    }
  }

  // Calculate internal time based on elapsed millis
  unsigned long elapsedSeconds = (millis() - internalTimeSetMillis) / 1000;
  time_t currentTime = internalTimeOffset + elapsedSeconds;
  struct tm *timeinfo = localtime(&currentTime);

  return DateTime(timeinfo->tm_year + 1900, timeinfo->tm_mon + 1,
                  timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min,
                  timeinfo->tm_sec);
}

// Set time on RTC or internal fallback
void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                int year) {
  if (rtcInitialized) {
    // Set time on external RTC
    rtc.adjust(DateTime(year, month, day, hours, minutes, seconds));
    Serial.printf("RTC Time set to: %04d-%02d-%02d %02d:%02d:%02d\n", year,
                  month, day, hours, minutes, seconds);
  } else {
    // Set internal time (fallback)
    struct tm newTime = {0};
    newTime.tm_year = year - 1900;
    newTime.tm_mon = month - 1; // 0-11
    newTime.tm_mday = day;
    newTime.tm_hour = hours;
    newTime.tm_min = minutes;
    newTime.tm_sec = seconds;
    internalTimeOffset = mktime(&newTime);
    internalTimeSetMillis = millis();

    Serial.printf("Internal Time set to: %04d-%02d-%02d %02d:%02d:%02d\n", year,
                  month, day, hours, minutes, seconds);
    Serial.println("  (Note: Time will be lost on power cycle)");
  }

  timeWasSet = true;
}
