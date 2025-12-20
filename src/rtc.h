#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

// RTC Module (DS1307)
extern RTC_DS1307 rtc;
extern bool rtcInitialized;

// I2C Pins for ESP32-C3
#define I2C_SDA 4
#define I2C_SCL 5

void setupRTC();
void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                int year);

// Global variable definitions
RTC_DS1307 rtc;
bool rtcInitialized = false;

void setupRTC() {
  Serial.println("=== RTC Setup ===");
  Serial.printf("  Module: DS1307\n");
  Serial.printf("  I2C Pins: SDA=GPIO%d, SCL=GPIO%d\n", I2C_SDA, I2C_SCL);

  // Initialize I2C with custom pins for ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!rtc.begin()) {
    Serial.println("  ERROR: RTC not found! Check wiring.");
    rtcInitialized = false;
    Serial.println("=================\n");
    return;
  }

  rtcInitialized = true;
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

// Set time on RTC
void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                int year) {
  if (!rtcInitialized) {
    Serial.println("RTC not initialized, cannot set time!");
    return;
  }

  rtc.adjust(DateTime(year, month, day, hours, minutes, seconds));
  timeWasSet = true;

  Serial.printf("RTC Time set to: %04d-%02d-%02d %02d:%02d:%02d\n", year, month,
                day, hours, minutes, seconds);
}
