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
// Global State
// ============================================================================
CRGB leds[NUM_LEDS];
Timer timer;
int8_t randomChangeEvent = -1;
int8_t sleepAgainEvent = -1;
int8_t autoWakeupEvent = -1;
Segment segments[NUM_SEGMENTS];
CHSV mainColor = CHSV(random(0, 255), 255, 255);
bool awake = false;
unsigned long lastMillis = millis();

// ============================================================================
// Include sub-modules (after globals are defined)
// ============================================================================
#include "display.h"
#include "wakeup.h"

// ============================================================================
// LED Setup
// ============================================================================
void setupLEDs() {
  Serial.println("Setup LEDs");

  // Initialize digit segments
  for (int i = 0; i < 7 * 4; i++) {
    int start = i * LEDS_PER_SEGMENT;
    start = start >= 140 ? start + COLON_LEDS : start; // Jump over Colon
    segments[i] = Segment(leds, start, LEDS_PER_SEGMENT);
  }

  // Initialize colon segment
  segments[COLON_INDEX] = Segment(leds, 140, COLON_LEDS);

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.showColor(CRGB::Black);

  // Schedule auto wakeup based on settings
  scheduleAutoWakeup();
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

  // Handle time not set state
  if (!timeWasSet) {
    setNumber(0000, 255);
    segments[COLON_INDEX].opacity = 255;
    CRGB color = ((millis() % 2000) > 1000) ? mainColor : CRGB(0, 0, 0);
    for (int i = 0; i < NUM_SEGMENTS; i++) {
      segments[i].fillColor(color, 255);
    }
  } else {
    // Check if display should be active using settings
    DateTime now = rtc.now();
    if (!isDisplayActiveTime(now.dayOfTheWeek(), now.hour())) {
      FastLED.showColor(CRGB::Black);
      return;
    }
  }

  // Handle wakeup transition
  if (wakeup && !awake) {
    startSleepTimer();
    awake = true;
    wakeup = false;
    showCurrentTime();
    mainColor = CHSV(random(0, 255), 255, 255);
    for (int i = 0; i < 7 * 4; i++) {
      segments[i].fillColor(mainColor, 1);
    }
    segments[COLON_INDEX].opacity = 255;
    segments[COLON_INDEX].fillColor(mainColor, 255);
  }

  // Continue showing time while awake
  if (awake) {
    showCurrentTime();
  }

  // Draw & Update Routine
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].draw();
  }
  timer.update();
  FastLED.show();
}
