#pragma once
#include <Arduino.h>
#include <RTClib.h>

// ===========================================================================
// Time
// ===========================================================================
//
// The ESP32 system clock is the single time base and holds UTC. The DS1307 is
// a seed at boot and a sink for NTP results, not a parallel source of truth —
// that is what let the timezone setting be stored but never applied.
//
// NOTE ON UPGRADING: the DS1307 now holds UTC, where earlier firmware stored
// local time. On the first boot after this change the displayed time may be off
// by the UTC offset until NTP syncs (automatic within seconds when a WiFi
// network is configured) or the time is set once via the web UI.

void setupClockTime();
void loopClockTime();

// Current local time, derived from the system clock and the configured
// timezone. Cheap to call every frame — the result is cached.
DateTime nowLocal();

// Set the clock from local wall-clock values (what the user types in the UI).
// Converts to UTC using the active timezone and persists to the RTC.
void setLocalTime(int year, int month, int day, int hour, int minute,
                  int second);

// Re-read the timezone from settings and apply it to the C library and SNTP.
void applyTimezone();

bool hasValidTime();
bool rtcPresent();
bool ntpSynced();
