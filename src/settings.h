#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Settings namespace for NVS storage
#define SETTINGS_NAMESPACE "clock-settings"

// Wakeup interval options in minutes
enum WakeupInterval {
  WAKEUP_5MIN = 5,
  WAKEUP_15MIN = 15,
  WAKEUP_30MIN = 30,
  WAKEUP_1H = 60,
  WAKEUP_2H = 120,
  WAKEUP_3H = 180,
  WAKEUP_4H = 240,
  WAKEUP_6H = 360,
  WAKEUP_OFF = 0 // No automatic wakeup
};

// Active hours for a single day
struct DaySchedule {
  bool enabled;      // Whether the display is active on this day
  uint8_t startHour; // Start hour (0-23)
  uint8_t endHour;   // End hour (0-23)
};

// Global settings structure
struct ClockSettings {
  DaySchedule days[7];     // 0=Sunday, 1=Monday, ... 6=Saturday
  uint16_t wakeupInterval; // Minutes between automatic wakeups (0=off)
  bool useActiveHours;     // Whether to use active hours at all
};

// Global settings instance
extern ClockSettings clockSettings;
extern Preferences preferences;

// Initialize settings from NVS
void setupSettings() {
  Serial.println("Setup Settings");
  preferences.begin(SETTINGS_NAMESPACE, false);

  // Load useActiveHours setting
  clockSettings.useActiveHours = preferences.getBool("useActiveHrs", true);

  // Load wakeup interval
  clockSettings.wakeupInterval = preferences.getUShort("wakeupInt", WAKEUP_OFF);

  // Load day schedules
  // Default: Mon-Fri 8-18, Sat-Sun off
  for (int i = 0; i < 7; i++) {
    String prefix = "day" + String(i);

    // Default values: Mon-Fri enabled 8-18, Sat-Sun disabled
    bool defaultEnabled = (i >= 1 && i <= 5); // Mon-Fri
    uint8_t defaultStart = 8;
    uint8_t defaultEnd = 18;

    clockSettings.days[i].enabled =
        preferences.getBool((prefix + "en").c_str(), defaultEnabled);
    clockSettings.days[i].startHour =
        preferences.getUChar((prefix + "st").c_str(), defaultStart);
    clockSettings.days[i].endHour =
        preferences.getUChar((prefix + "ed").c_str(), defaultEnd);
  }

  Serial.println("Settings loaded from NVS");
}

// Save all settings to NVS
void saveSettings() {
  preferences.putBool("useActiveHrs", clockSettings.useActiveHours);
  preferences.putUShort("wakeupInt", clockSettings.wakeupInterval);

  for (int i = 0; i < 7; i++) {
    String prefix = "day" + String(i);
    preferences.putBool((prefix + "en").c_str(), clockSettings.days[i].enabled);
    preferences.putUChar((prefix + "st").c_str(),
                         clockSettings.days[i].startHour);
    preferences.putUChar((prefix + "ed").c_str(),
                         clockSettings.days[i].endHour);
  }

  Serial.println("Settings saved to NVS");
}

// Save only active hours settings
void saveActiveHours() {
  preferences.putBool("useActiveHrs", clockSettings.useActiveHours);

  for (int i = 0; i < 7; i++) {
    String prefix = "day" + String(i);
    preferences.putBool((prefix + "en").c_str(), clockSettings.days[i].enabled);
    preferences.putUChar((prefix + "st").c_str(),
                         clockSettings.days[i].startHour);
    preferences.putUChar((prefix + "ed").c_str(),
                         clockSettings.days[i].endHour);
  }

  Serial.println("Active hours saved");
}

// Save only wakeup interval
void saveWakeupInterval() {
  preferences.putUShort("wakeupInt", clockSettings.wakeupInterval);
  Serial.printf("Wakeup interval saved: %d minutes\n",
                clockSettings.wakeupInterval);
}

// Check if display should be active right now based on settings
bool isDisplayActiveTime(uint8_t weekday, uint8_t hour) {
  if (!clockSettings.useActiveHours) {
    return true; // Always active if feature disabled
  }

  DaySchedule &day = clockSettings.days[weekday];

  if (!day.enabled) {
    return false;
  }

  // Handle same-day schedule (e.g., 8-18)
  if (day.startHour <= day.endHour) {
    return (hour >= day.startHour && hour < day.endHour);
  }

  // Handle overnight schedule (e.g., 22-6) - not typical but supported
  return (hour >= day.startHour || hour < day.endHour);
}

// Global variable definitions
ClockSettings clockSettings;
Preferences preferences;
