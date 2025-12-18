#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>

#include "definitions.h"

// RTC Module (DS3231)
extern RTC_DS3231 rtc;
extern bool rtcInitialized;

// I2C Pins for ESP32-C3
#define I2C_SDA 4
#define I2C_SCL 5

void setupRTC();
void setRTCTime(int hours, int minutes, int seconds, int day, int month, int year);

// Global variable definitions
RTC_DS3231 rtc;
bool rtcInitialized = false;

void setupRTC() {
  Serial.println("Setup RTC");
  
  // Initialize I2C with custom pins for ESP32-C3
  Wire.begin(I2C_SDA, I2C_SCL);
  
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC!");
    rtcInitialized = false;
    return;
  }
  
  rtcInitialized = true;
  Serial.println("RTC initialized!");
  
  // Check if RTC lost power and needs to be set
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, needs to be set via web interface!");
    timeWasSet = false;
  } else {
    // RTC has valid time
    timeWasSet = true;
    DateTime now = rtc.now();
    Serial.printf("RTC Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
  }
}

// Set time on RTC
void setRTCTime(int hours, int minutes, int seconds, int day, int month, int year) {
  if (!rtcInitialized) {
    Serial.println("RTC not initialized, cannot set time!");
    return;
  }
  
  rtc.adjust(DateTime(year, month, day, hours, minutes, seconds));
  timeWasSet = true;
  
  Serial.printf("RTC Time set to: %04d-%02d-%02d %02d:%02d:%02d\n", 
                year, month, day, hours, minutes, seconds);
}
