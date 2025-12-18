#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <RTClib.h>
#include <SPI.h>
#include <Timer.h>

#include "definitions.h"
#include "segment.h"

// RTC from rtc.h
extern RTC_DS3231 rtc;
extern bool rtcInitialized;

#define FRAMES_PER_SECOND 60
#define DATA_PIN 6  // GPIO6 on ESP32-C3
#define CLOCK_PIN 7 // GPIO7 on ESP32-C3
#define NUM_LEDS 282

CRGB leds[NUM_LEDS];
Timer timer;
int8_t randomChangeEvent = -1;
int8_t sleepAgainEvent = -1;
Segment segments[7 * 4 + 1];
CHSV mainColor = CHSV(random(0, 255), 255, 255);
bool awake = false;

unsigned long lastMillis = millis();

void setDigit(int position, int value, int opacity) {
  // Array for positions of segments
  bool sevenSegmentDigits[10][7] = {
      {1, 1, 1, 0, 1, 1, 1}, // = 0
      {0, 0, 1, 0, 0, 0, 1}, // = 1
      {1, 1, 0, 1, 0, 1, 1}, // = 2
      {0, 1, 1, 1, 0, 1, 1}, // = 3
      {0, 0, 1, 1, 1, 0, 1}, // = 4
      {0, 1, 1, 1, 1, 1, 0}, // = 5
      {1, 1, 1, 1, 1, 1, 0}, // = 6
      {0, 0, 1, 0, 0, 1, 1}, // = 7
      {1, 1, 1, 1, 1, 1, 1}, // = 8
      {0, 1, 1, 1, 1, 1, 1}  // = 9
  };
  // Set opacity of 7 segments starting in startposition
  int start = position * 7;
  for (int segPos = 0; segPos < 7; segPos++) {
    if (sevenSegmentDigits[value][segPos]) {
      segments[start + segPos].opacity = opacity;
    }
  }
}

void setNumber(int value, int opacity) {
  // Convenience function to set a number
  const int numDigits = 4;
  for (int i = 0; i < numDigits; i++) {
    setDigit(i, value / ((int)pow(10, numDigits - 1 - i)) % 10, opacity);
  }
}

void showCurrentTime() {
  // First set all digits to zero opacity, then set the time
  DateTime now = rtc.now();
  setNumber(8888, 0);
  setNumber(now.minute() + now.hour() * 100, 255);
  CRGB blinkingColon = ((millis() % 2000) > 1000) ? mainColor : CRGB(0, 0, 0);
  segments[28].fillColor(blinkingColon, 255);
}

void goSleep() {
  // Go sleep again, after end of timer
  awake = false;
  for (int i = 0; i < (7 * 4 + 1); i++) {
    segments[i].mode = SegmentMode::RANDOM;
    segments[i].speed = 1;
  }
}

void setupLEDs() {
  Serial.println("Setup LEDs");
  int length = 10;
  for (int i = 0; i < 7 * 4; i++) {
    int start = i * length;
    start = start >= 140 ? start + 2 : start; // Jump over Colon
    segments[i] = Segment(leds, start, length);
  }
  segments[28] = Segment(leds, 140, 2);

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  // FastLED.setMaxPowerInMilliWatts(1000);
  FastLED.showColor(CRGB::Black);
}

void loopLEDs() {
  // Only execute very x FPS
  if ((millis() - lastMillis) < (1000 / FRAMES_PER_SECOND)) {
    return;
  }
  lastMillis = millis();

  if (!timeWasSet) {
    // Time was not set, animate blinking 00:00
    setNumber(0000, 255);
    segments[28].opacity = 255;
    CRGB color = ((millis() % 2000) > 1000) ? mainColor : CRGB(0, 0, 0);
    for (int i = 0; i < 7 * 4 + 1; i++) {
      segments[i].fillColor(color, 255);
    }
  } else {
    DateTime now = rtc.now();
    if (ONLY_OFFICE_HOURS &&
        !(now.hour() > 8 && now.hour() < 18 &&
          now.dayOfTheWeek() > 0 && now.dayOfTheWeek() < 6)) {
      // only display during 9-17 Mon-Fri (weekday: 0=Sun, 1=Mon, ... 6=Sat)
      FastLED.showColor(CRGB::Black);
      return;
    }
  }

  if (wakeup && !awake) {
    // waking up, happens once
    if (sleepAgainEvent >= 0) {
      timer.stop(sleepAgainEvent);
    }
    sleepAgainEvent = timer.after(15000, goSleep);
    awake = true;
    wakeup = false;
    showCurrentTime();
    mainColor = CHSV(random(0, 255), 255, 255);
    for (int i = 0; i < 7 * 4; i++) {
      segments[i].fillColor(mainColor, 1);
    }
    segments[28].opacity = 255;
    segments[28].fillColor(mainColor, 255);
  }

  if (awake) {
    // repeat for x seconds
    showCurrentTime();
  }

  // Draw & Update Routine
  for (int i = 0; i < 7 * 4 + 1; i++) {
    segments[i].draw();
  }
  timer.update();
  FastLED.show();
}
