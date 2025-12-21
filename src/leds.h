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
// LED Hardware Configuration
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
// Global Hardware State
// ============================================================================
CRGB leds[NUM_LEDS];
Timer timer;
Segment segments[NUM_SEGMENTS];
unsigned long lastMillis = millis();

// ============================================================================
// Include mode management (after hardware globals are defined)
// ============================================================================
#include "modes.h"

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

  // Update display mode state machine
  updateMode();

  // Skip rendering if display is off
  if (currentMode == MODE_OFF) {
    return;
  }

  // Draw all segments & update timers
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].draw();
  }
  timer.update();
  FastLED.show();
}
