#pragma once
#include <Arduino.h>
#include <Timer.h>

#include "segment.h"
#include "settings.h"

// Constants
#define WAKEUP_DURATION_MS 15000

// External references
extern Timer timer;
extern Segment segments[];
extern bool awake;
extern bool wakeup;
extern bool timeWasSet;
extern ClockSettings clockSettings;

// Timer event IDs
extern int8_t sleepAgainEvent;
extern int8_t autoWakeupEvent;

// Forward declaration
void scheduleAutoWakeup();

// Go back to sleep after wakeup duration
inline void goSleep() {
  awake = false;
  for (int i = 0; i < (7 * 4 + 1); i++) {
    segments[i].mode = SegmentMode::RANDOM;
    segments[i].speed = 1;
  }
}

// Trigger automatic wakeup
inline void triggerAutoWakeup() {
  wakeup = true;
  scheduleAutoWakeup(); // Schedule next wakeup
}

// Schedule the next auto wakeup based on settings
inline void scheduleAutoWakeup() {
  if (autoWakeupEvent >= 0) {
    timer.stop(autoWakeupEvent);
    autoWakeupEvent = -1;
  }

  int intervalMinutes = clockSettings.wakeupInterval;
  if (intervalMinutes > 0 && timeWasSet) {
    unsigned long intervalMs = (unsigned long)intervalMinutes * 60 * 1000;
    autoWakeupEvent = timer.after(intervalMs, triggerAutoWakeup);
  }
}

// Start the sleep timer after wakeup
inline void startSleepTimer() {
  if (sleepAgainEvent >= 0) {
    timer.stop(sleepAgainEvent);
  }
  sleepAgainEvent = timer.after(WAKEUP_DURATION_MS, goSleep);
}
