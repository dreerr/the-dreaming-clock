#pragma once
#include <stdint.h>

// ===========================================================================
// Display modes
// ===========================================================================

enum class DisplayMode : uint8_t {
  OFF,          // outside active hours
  TIME_NOT_SET, // blinking 00:00
  DREAM,        // drifting noise with words condensing out of it
  PATTERN,      // drifting noise, no words
  WAKEUP,       // the time, shown solidly
  MESSAGE,      // a queued message, playing over everything else
};

const char *displayModeName(DisplayMode mode);

void setupModes(uint32_t now);

// Runs the mode state machine and services the timers. Called every frame,
// including while the display is off — the timers must keep running or a
// wakeup scheduled during off-hours fires late.
void updateMode(uint32_t now);

DisplayMode currentDisplayMode();

// Ask for the time to be shown now. Safe to call from a web handler; the
// request is picked up on the next frame.
void requestWakeup();

// Recompute the next automatic wakeup from the wall clock. Call after the time
// or the interval changes.
void scheduleAutoWakeup();

// Force dream or pattern mode (used by the API).
void enterDreamMode(uint32_t now);
void enterPatternMode(uint32_t now);

// Start playing whatever is queued in messages.h. A message overrides every
// other mode, including quiet hours and a clock that does not know the time,
// and hands the display back to whatever it interrupted.
void startMessages();
