#pragma once
#include <Arduino.h>
#include <stdint.h>

#include "config.h"
#include "schedule.h"

// ===========================================================================
// Persistent settings (NVS)
// ===========================================================================

enum WakeupInterval : uint16_t {
  WAKEUP_OFF = 0,
  WAKEUP_5MIN = 5,
  WAKEUP_15MIN = 15,
  WAKEUP_30MIN = 30,
  WAKEUP_1H = 60,
  WAKEUP_2H = 120,
  WAKEUP_3H = 180,
  WAKEUP_4H = 240,
  WAKEUP_6H = 360,
};

enum NetworkMode : uint8_t {
  NETWORK_CAPTIVE = 0,
  NETWORK_CLIENT = 1,
};

struct NetworkSettings {
  NetworkMode mode;
  char ssid[33];     // 32 chars + NUL
  char password[65]; // 64 chars + NUL
  bool fallbackToCaptive;
};

struct ClockSettings {
  DaySchedule days[7]; // 0 = Sunday .. 6 = Saturday
  uint16_t wakeupInterval;
  bool useActiveHours;
  char timezone[40];      // IANA name, e.g. "Europe/Vienna"
  uint8_t brightness;     // ceiling while showing the time
  uint8_t dreamBrightness; // ceiling while dreaming
};

extern ClockSettings clockSettings;
extern NetworkSettings networkSettings;

void setupSettings();

void saveActiveHours();
void saveWakeupInterval();
void saveTimezone();
void saveBrightness();
void saveNetworkSettings();

// Convenience wrapper around isDisplayActiveTime() bound to the live settings.
bool isDisplayActiveNow(uint8_t weekday, uint8_t hour);
