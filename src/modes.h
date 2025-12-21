#pragma once
#include <Arduino.h>
#include <RTClib.h>
#include <Timer.h>

#include "display.h"
#include "dreams.h"
#include "segment.h"
#include "settings.h"

// ============================================================================
// External References
// ============================================================================
extern RTC_DS1307 rtc;
extern bool rtcInitialized;
extern bool usingInternalTime;
DateTime getCurrentTime();
extern CRGB leds[];
extern Segment segments[];
extern Timer timer;

// ============================================================================
// Constants
// ============================================================================
#define WAKEUP_DURATION_MS 15000

// ============================================================================
// Display Modes
// ============================================================================
enum DisplayMode {
  MODE_OFF,          // Display is off (outside active hours)
  MODE_TIME_NOT_SET, // Blinking 00:00
  MODE_DREAM,        // Random colors with subliminal words
  MODE_WAKEUP,       // Showing current time clearly
};

// ============================================================================
// Mode State
// ============================================================================
DisplayMode currentMode = MODE_DREAM;
bool awake = false;
CHSV mainColor = CHSV(random(0, 255), 255, 255);

// Timer event IDs
int8_t sleepAgainEvent = -1;
int8_t autoWakeupEvent = -1;
int8_t dreamWordEvent = -1;

// Dream word state
bool showingDreamWord = false;
const char *currentDreamWord = nullptr;
unsigned long dreamWordStartTime = 0;
int dreamWordOpacity = 0;

// ============================================================================
// Forward Declarations
// ============================================================================
void enterDreamMode();
void enterWakeupMode();
void scheduleAutoWakeup();
void startDreamWord();
void endDreamWord();

// ============================================================================
// Timer Callbacks
// ============================================================================

// Go back to dream mode after wakeup duration
inline void goSleep() { enterDreamMode(); }

// Trigger automatic wakeup
inline void triggerAutoWakeup() {
  wakeup = true;
  scheduleAutoWakeup();
}

// ============================================================================
// Timer Scheduling
// ============================================================================

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

// ============================================================================
// Dream Word Functions
// ============================================================================

// Set a word on the display with given opacity
inline void setDreamWord(const char *word, int opacity) {
  if (word == nullptr)
    return;

  int len = strlen(word);
  for (int i = 0; i < 4; i++) {
    if (i < len) {
      setChar(i, word[i], opacity);
      printf(" %c", word[i]);
    } else {
      setChar(i, ' ', 0);
    }
  }
  // Keep colon dim during dreams
  segments[COLON_INDEX].opacity = opacity / 4;
}

// Start showing a new dream word
void startDreamWord() {
  if (random8() > DREAM_WORD_PROBABILITY) {
    // Skip this word, schedule next attempt
    dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS / 2, startDreamWord);
    return;
  }

  currentDreamWord = getRandomDreamWord();
  printf("Dream Word: %s\n", currentDreamWord);
  showingDreamWord = true;
  dreamWordStartTime = millis();
  dreamWordOpacity = DREAM_WORD_MIN_OPACITY;

  // Schedule end of word display
  dreamWordEvent = timer.after(DREAM_WORD_DISPLAY_MS, endDreamWord);
}

// End the current dream word and return to pure random
void endDreamWord() {
  showingDreamWord = false;
  currentDreamWord = nullptr;
  dreamWordOpacity = 0;

  // Return all segments to random mode
  for (int i = 0; i < 7 * 4; i++) {
    segments[i].mode = SegmentMode::RANDOM;
  }

  // Schedule next dream word
  dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS, startDreamWord);
}

// Update dream word animation (call every frame)
void updateDreamWord() {
  if (!showingDreamWord || currentDreamWord == nullptr)
    return;

  unsigned long elapsed = millis() - dreamWordStartTime;
  unsigned long fadeTime = DREAM_WORD_DISPLAY_MS / 4; // 25% fade in/out

  if (elapsed < fadeTime) {
    // Fading in
    dreamWordOpacity = map(elapsed, 0, fadeTime, DREAM_WORD_MIN_OPACITY,
                           DREAM_WORD_MAX_OPACITY);
  } else if (elapsed > DREAM_WORD_DISPLAY_MS - fadeTime) {
    // Fading out
    unsigned long fadeElapsed = elapsed - (DREAM_WORD_DISPLAY_MS - fadeTime);
    dreamWordOpacity = map(fadeElapsed, 0, fadeTime, DREAM_WORD_MAX_OPACITY,
                           DREAM_WORD_MIN_OPACITY);
  } else {
    // Full visibility (but still subtle)
    dreamWordOpacity = DREAM_WORD_MAX_OPACITY;
  }

  // Apply the word pattern with current opacity
  setDreamWord(currentDreamWord, dreamWordOpacity);

  // Set colors for visible segments - blend with random background
  for (int i = 0; i < 7 * 4; i++) {
    if (segments[i].opacity > 0) {
      CHSV dreamColor = CHSV((millis() / 100) % 255, 180, dreamWordOpacity);
      segments[i].fillColor(dreamColor, DREAM_WORD_FADE_SPEED);
    }
  }
}

// ============================================================================
// Mode Handlers
// ============================================================================

// Handle MODE_TIME_NOT_SET - blinking zeros
void handleTimeNotSet() {
  setNumber(0, 255);
  segments[COLON_INDEX].opacity = 255;
  CRGB color = ((millis() % 2000) > 1000) ? mainColor : CRGB(0, 0, 0);
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].fillColor(color, 255);
  }
}

// Handle MODE_DREAM - random patterns with subliminal words
void handleDreamMode() { updateDreamWord(); }

// Handle MODE_WAKEUP - show current time clearly
void handleWakeupMode() { showCurrentTime(); }

// ============================================================================
// Mode Transitions
// ============================================================================

// Transition to dream mode
void enterDreamMode() {
  currentMode = MODE_DREAM;
  awake = false;

  // Reset all segments to random
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].mode = SegmentMode::RANDOM;
    segments[i].speed = random(MIN_SPEED, MAX_SPEED);
  }

  // Start the dream word cycle
  if (dreamWordEvent >= 0) {
    timer.stop(dreamWordEvent);
  }
  showingDreamWord = false;
  dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS, startDreamWord);
}

// Transition to wakeup mode (showing time)
void enterWakeupMode() {
  currentMode = MODE_WAKEUP;
  awake = true;

  // Stop any pending dream word
  if (dreamWordEvent >= 0) {
    timer.stop(dreamWordEvent);
    dreamWordEvent = -1;
  }
  showingDreamWord = false;

  // Pick new main color
  mainColor = CHSV(random(0, 255), 255, 255);

  // Set all digit segments to show time with color mode
  for (int i = 0; i < 7 * 4; i++) {
    segments[i].fillColor(mainColor, DREAM_WORD_FADE_SPEED);
  }
  segments[COLON_INDEX].opacity = 255;
  segments[COLON_INDEX].fillColor(mainColor, 255);

  // Start sleep timer
  startSleepTimer();
}

// ============================================================================
// Mode Update (called from main loop)
// ============================================================================
void updateMode() {
  // Determine current mode based on state
  if (!timeWasSet) {
    currentMode = MODE_TIME_NOT_SET;
  } else {
    // Check if display should be active using settings
    DateTime now = getCurrentTime();
    if (!isDisplayActiveTime(now.dayOfTheWeek(), now.hour())) {
      currentMode = MODE_OFF;
      FastLED.showColor(CRGB::Black);
      return;
    }
  }

  // Handle wakeup trigger
  if (wakeup && currentMode != MODE_WAKEUP) {
    wakeup = false;
    enterWakeupMode();
    showCurrentTime();
  }

  // Execute current mode handler
  switch (currentMode) {
  case MODE_TIME_NOT_SET:
    handleTimeNotSet();
    break;
  case MODE_DREAM:
    handleDreamMode();
    break;
  case MODE_WAKEUP:
    handleWakeupMode();
    break;
  case MODE_OFF:
    // Already handled above
    break;
  }
}
