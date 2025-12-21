#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <time.h>

// RTC Module (DS1307)
extern RTC_DS1307 rtc;
extern bool rtcInitialized;
extern bool usingInternalTime;

// I2C Pins for ESP32-C3
#define I2C_SDA 4
#define I2C_SCL 5

void setupRTC();
void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                int year);
DateTime getCurrentTime();

// Global variable definitions
RTC_DS1307 rtc;
bool rtcInitialized = false;
bool usingInternalTime = false;

// Internal time tracking (fallback when RTC not connected)
static time_t internalTimeOffset = 0;
static unsigned long internalTimeSetMillis = 0;

void setupRTC() {
  Serial.println("=== RTC Setup ===");
  Serial.printf("  Module: DS1307\n");
  Serial.printf("  I2C Pins: SDA=GPIO%d, SCL=GPIO%d\n", I2C_SDA, I2C_SCL);

  // Initialize I2C with custom pins for ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    Serial.println("  WARNING: RTC not found! Using internal time.");
    Serial.println("  -> Time will be lost on power cycle");
    rtcInitialized = false;
    usingInternalTime = true;

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
    Serial.println("  -> Set time via web interface: /settings");
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
