#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <RTClib.h>
#include <SPI.h>
#include <Timer.h>

#include "segment.h"
#include "settings.h"

// RTC from rtc.h
extern RTC_DS1307 rtc;
extern bool rtcInitialized;

// ============================================================================
// LED Configuration
// ============================================================================
#define FRAMES_PER_SECOND 60
#define DATA_PIN 6  // GPIO6 on ESP32-C3
#define CLOCK_PIN 7 // GPIO7 on ESP32-C3
#define NUM_LEDS 282
#define NUM_SEGMENTS (7 * 4 + 1) // 4 digits + colon
#define LEDS_PER_SEGMENT 10
#define COLON_LEDS 2
#define COLON_INDEX 28

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
// Global State
// ============================================================================
CRGB leds[NUM_LEDS];
Timer timer;
int8_t randomChangeEvent = -1;
int8_t sleepAgainEvent = -1;
int8_t autoWakeupEvent = -1;
int8_t dreamWordEvent = -1;
Segment segments[NUM_SEGMENTS];
CHSV mainColor = CHSV(random(0, 255), 255, 255);
unsigned long lastMillis = millis();

// Display state
DisplayMode currentMode = MODE_DREAM;
bool awake = false;

// Dream word state
bool showingDreamWord = false;
const char *currentDreamWord = nullptr;
unsigned long dreamWordStartTime = 0;
int dreamWordOpacity = 0;
bool dreamWordFadingIn = true;

// ============================================================================
// Include sub-modules (after globals are defined)
// ============================================================================
#include "display.h"
#include "dreams.h"
#include "wakeup.h"

// ============================================================================
// Dream Word Functions
// ============================================================================

// Set a word on the display with given opacity (for dream phase)
inline void setDreamWord(const char *word, int opacity) {
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

// Forward declarations
void endDreamWord();
void startDreamWord();

// Start showing a new dream word
void startDreamWord() {
  if (random8() > DREAM_WORD_PROBABILITY) {
    // Skip this word, schedule next attempt
    dreamWordEvent = timer.after(DREAM_WORD_PAUSE_MS / 2, startDreamWord);
    return;
  }

  currentDreamWord = getRandomDreamWord();
  showingDreamWord = true;
  dreamWordStartTime = millis();
  dreamWordOpacity = DREAM_WORD_MIN_OPACITY;
  dreamWordFadingIn = true;

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
      // Use a dreamy color that changes slowly
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
void handleDreamMode() {
  // Dream words are handled by updateDreamWord() which is timer-based
  // Segments not showing words remain in RANDOM mode
  updateDreamWord();
}

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
// LED Setup
// ============================================================================
void setupLEDs() {
  Serial.println("=== LED Setup ===");
  Serial.printf("  LED Type: APA102 (Dotstar)\n");
  Serial.printf("  Total LEDs: %d\n", NUM_LEDS);
  Serial.printf("  Data Pin: GPIO%d, Clock Pin: GPIO%d\n", DATA_PIN, CLOCK_PIN);
  Serial.printf("  Segments: %d (4 digits × 7 + colon)\n", NUM_SEGMENTS);
  Serial.printf("  LEDs per segment: %d\n", LEDS_PER_SEGMENT);

  // Initialize digit segments
  for (int i = 0; i < 7 * 4; i++) {
    int start = i * LEDS_PER_SEGMENT;
    start = start >= 140 ? start + COLON_LEDS : start; // Jump over Colon
    segments[i] = Segment(leds, start, LEDS_PER_SEGMENT);
  }

  // Initialize colon segment
  segments[COLON_INDEX] = Segment(leds, 140, COLON_LEDS);
  Serial.println("  Segments initialized");

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.showColor(CRGB::Black);
  Serial.printf("  FastLED initialized @ %d FPS\n", FRAMES_PER_SECOND);

  // Start in dream mode
  enterDreamMode();

  // Schedule auto wakeup based on settings
  scheduleAutoWakeup();
  Serial.println("=================\n");
}

// ============================================================================
// LED Main Loop
// ============================================================================
void loopLEDs() {
  // Frame rate limiting
  if ((millis() - lastMillis) < (1000 / FRAMES_PER_SECOND)) {
    return;
  }
  lastMillis = millis();

  // Determine current mode based on state
  if (!timeWasSet) {
    currentMode = MODE_TIME_NOT_SET;
  } else if (rtcInitialized) {
    // Check if display should be active using settings
    DateTime now = rtc.now();
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

  // Draw & Update Routine
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].draw();
  }
  timer.update();
  FastLED.show();
}
