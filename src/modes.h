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

// Schedule the next auto wakeup at the next aligned time
// e.g., every 15 min -> wakeup at :00, :15, :30, :45
inline void scheduleAutoWakeup() {
  if (autoWakeupEvent >= 0) {
    timer.stop(autoWakeupEvent);
    autoWakeupEvent = -1;
  }

  int intervalMinutes = clockSettings.wakeupInterval;
  if (intervalMinutes <= 0 || !timeWasSet) {
    return;
  }

  DateTime now = getCurrentTime();
  int currentMinute = now.minute();
  int currentSecond = now.second();

  // Calculate minutes until next aligned time
  // e.g., if interval=15 and current=17, next aligned = 30, wait = 13 min
  int minutesSinceLastSlot = currentMinute % intervalMinutes;
  int minutesToNextSlot = intervalMinutes - minutesSinceLastSlot;

  // If we're exactly on a slot, schedule for the next one
  if (minutesSinceLastSlot == 0 && currentSecond == 0) {
    minutesToNextSlot = intervalMinutes;
  }

  // Convert to milliseconds, accounting for current seconds
  unsigned long msToNextSlot =
      (unsigned long)minutesToNextSlot * 60 * 1000 - currentSecond * 1000;

  int nextMinute = (currentMinute + minutesToNextSlot) % 60;
  Serial.printf("[WAKEUP] Next auto wakeup at :%02d (in %lu ms)\n", nextMinute,
                msToNextSlot);

  autoWakeupEvent = timer.after(msToNextSlot, triggerAutoWakeup);
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
  // return;
  if (word == nullptr)
    return;

  int len = strlen(word);
  for (int i = 0; i < 4; i++) {
    if (i < len) {
      setChar(i, word[i], opacity);
    } else {
      setChar(i, ' ', 0);
    }
  }
  // Keep colon dim during dreams
  segments[COLON_INDEX].opacity = opacity / 4;
}

// Start showing a new dream word
void startDreamWord() {
  // return;
  // Only start dream word if we're actually in dream mode
  if (currentMode != MODE_DREAM) {
    Serial.println("[DREAM] Skipping dream word - not in dream mode");
    return;
  }

  if (random8() > DREAM_WORD_PROBABILITY) {
    // Skip this word, schedule next attempt
    Serial.println("[DREAM] Probability skip, trying again later");
    dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS / 2, startDreamWord);
    return;
  }

  currentDreamWord = getRandomDreamWord();
  Serial.printf("[DREAM] Starting word: %s\n", currentDreamWord);
  showingDreamWord = true;
  dreamWordStartTime = millis();
  dreamWordOpacity = DREAM_WORD_MIN_OPACITY;

  // Schedule end of word display
  dreamWordEvent = timer.after(DREAM_WORD_DISPLAY_MS, endDreamWord);
}

// End the current dream word and return to pure random
void endDreamWord() {
  // return;
  Serial.println("[DREAM] Ending dream word, returning to random");
  showingDreamWord = false;
  currentDreamWord = nullptr;
  dreamWordOpacity = 0;

  // Return all segments to random mode with full opacity
  for (int i = 0; i < 7 * 4; i++) {
    segments[i].mode = SegmentMode::RANDOM;
    segments[i].opacity = 255;
  }

  // Schedule next dream word (only if still in dream mode)
  if (currentMode == MODE_DREAM) {
    Serial.printf("[DREAM] Scheduling next word in %d ms\n",
                  DREAM_WORD_PAUSE_MS);
    dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS, startDreamWord);
  }
}

// Update dream word animation (call every frame)
void updateDreamWord() {
  // return;
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
  Serial.println("[DREAM] Entering dream mode");
  currentMode = MODE_DREAM;
  awake = false;

  // Reset all segments to random
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].mode = SegmentMode::RANDOM;
    segments[i].speed = random(MIN_SPEED, MAX_SPEED);
    segments[i].opacity = 255; // Ensure segments are visible
  }

  // Start the dream word cycle
  if (dreamWordEvent >= 0) {
    timer.stop(dreamWordEvent);
  }
  showingDreamWord = false;
  currentDreamWord = nullptr;

  // Schedule first dream word
  Serial.printf("[DREAM] Scheduling first dream word in %d ms\n",
                DREAM_WORD_PAUSE_MS);
  dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS, startDreamWord);
}

// Transition to wakeup mode (showing time)
void enterWakeupMode() {
  Serial.println("[WAKEUP] Entering wakeup mode");
  currentMode = MODE_WAKEUP;
  awake = true;

  // Stop any pending dream word
  if (dreamWordEvent >= 0) {
    timer.stop(dreamWordEvent);
    dreamWordEvent = -1;
  }
  showingDreamWord = false;
  currentDreamWord = nullptr;

  // Pick new main color
  mainColor = CHSV(random(0, 255), 255, 255);
  Serial.printf("[WAKEUP] Main color hue: %d\n", mainColor.hue);

  // Show current time immediately
  showCurrentTime();

  // Set all digit segments to COLOR mode first
  for (int i = 0; i < 7 * 4; i++) {
    segments[i].mode = SegmentMode::COLOR;
    segments[i].fillColor(mainColor, 10);
  }

  // Set colon to full brightness
  segments[COLON_INDEX].mode = SegmentMode::COLOR;
  segments[COLON_INDEX].opacity = 255;
  segments[COLON_INDEX].fillColor(mainColor, 255);

  // Start sleep timer
  Serial.printf("[WAKEUP] Sleep timer: %d ms\n", WAKEUP_DURATION_MS);
  startSleepTimer();
}

// ============================================================================
// Mode Update (called from main loop)
// ============================================================================
void updateMode() {
  // Handle wakeup trigger FIRST - before any mode checks
  // This ensures wakeup always takes priority
  if (wakeup) {
    wakeup = false;

    // Only wakeup if time is set and display should be active
    if (timeWasSet) {
      DateTime now = getCurrentTime();
      if (isDisplayActiveTime(now.dayOfTheWeek(), now.hour())) {
        enterWakeupMode();
        return; // Skip rest of update, mode is set
      }
    }
  }

  // Determine current mode based on state
  if (!timeWasSet) {
    if (currentMode != MODE_TIME_NOT_SET) {
      Serial.println("[MODE] Time not set -> MODE_TIME_NOT_SET");
      currentMode = MODE_TIME_NOT_SET;
    }
  } else if (currentMode != MODE_WAKEUP) {
    // Only check active hours if not in wakeup mode
    DateTime now = getCurrentTime();
    if (!isDisplayActiveTime(now.dayOfTheWeek(), now.hour())) {
      if (currentMode != MODE_OFF) {
        Serial.println("[MODE] Outside active hours -> MODE_OFF");
        currentMode = MODE_OFF;
      }
      FastLED.showColor(CRGB::Black);
      return;
    } else if (currentMode == MODE_OFF) {
      // Came back into active hours, go to dream mode
      Serial.println("[MODE] Back in active hours -> MODE_DREAM");
      enterDreamMode();
    }
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
