#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <FastLED.h>
#include <TimeLib.h>
#include <Timer.h>

#include "definitions.h"
#include "segment.h"

#define FRAMES_PER_SECOND 60
#define DATA_PIN 5
#define CLOCK_PIN 7

CRGB leds[NUM_LEDS];
Timer t;
int8_t randomChangeEvent = -1;
int8_t sleepAgainEvent = -1;
Segment segs[7*4];
bool awake = false;
CRGB not_set_color = CRGB(255, 0, 15);

unsigned long last_millis = millis();

void setDecimal(int position, int value, int amount) {
  // Array for positions of segments
  bool seven_seg_digits[10][7] = {
      {1, 1, 1, 0, 1, 1, 1},  // = 0
      {0, 0, 1, 0, 0, 0, 1},  // = 1
      {1, 1, 0, 1, 0, 1, 1},  // = 2
      {0, 1, 1, 1, 0, 1, 1},  // = 3
      {0, 0, 1, 1, 1, 0, 1},  // = 4
      {0, 1, 1, 1, 1, 1, 0},  // = 5
      {1, 1, 1, 1, 1, 1, 0},  // = 6
      {0, 0, 1, 0, 0, 1, 1},  // = 7
      {1, 1, 1, 1, 1, 1, 1},  // = 8
      {0, 1, 1, 1, 1, 1, 1}   // = 9
  };
  // Set opacity of 7 segments starting in startposition
  int start = position * 7;
  for (byte segPos = 0; segPos < 7; segPos++) {
    if (seven_seg_digits[value][segPos]) {
      segs[start + segPos].opacity = amount;
    }
  }
}

void setNumber(int value, int amount) {
  // Convenience function to set a number
  const int numDigits = 4;
  for (int i = 0; i < numDigits; i++) {
    setDecimal(i, value / ((int)pow(10, numDigits - 1 - i)) % 10, amount);
  }
}

void randomness() {
  // NOTE: Not in use anymore
  if(!awake && time_was_set) {
    for (int i = 0; i < 7 * 4; i++) {
      //segs[i].setRandom();
    }
  }
}

void showCurrentTime() {
  // First set all digits to zero opacity, then set the time
  setNumber(8888, 0);
  setNumber(minute() + hour() * 100, 255);
}

void goSleep() {
  // Go sleep again, after end of timer
  awake = false;
  for (int i = 0; i < 7 * 4; i++) {
    segs[i].mode = SegmentMode::RANDOM;
    segs[i].speed = 1;
  }
}

void setupLEDs() {
  DEBUG.printf("Setup LEDs");
  int length = 10;
  for (int i = 0; i < 7 * 4; i++) {
    int start = i * length;
    start = start > 130 ? start + 2 : start;  // Jump over Doppelpunkt
    segs[i] = Segment(leds, start, length);
  }

  FastLED.addLeds<APA102, DATA_PIN, CLOCK_PIN, BGR>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  // FastLED.setMaxPowerInMilliWatts(1000);
  FastLED.showColor(CRGB::Black);
  //randomChangeEvent = t.every(3000, randomness);
}

void loopLEDs() {
  // Only execute very x FPS
  if((millis() - last_millis) < (1000 / FRAMES_PER_SECOND)) return;
  last_millis = millis();


  if (!time_was_set) {
    // Time was not set, animate blinking 00:00
    setNumber(0000,255);
    CRGB color =
        ((millis() % 2000) > 1000) ? CRGB(255, 0, 15) : CRGB(0, 0, 0);
    if (color!=not_set_color) {
      for (int i = 0; i < 7 * 4; i++) {
        segs[i].fillColor(color, 10);
      }
      not_set_color = color;
    }
  }
  if (wakeup && !awake) {  // happens once
    if (sleepAgainEvent >= 0) {
      t.stop(sleepAgainEvent);
    }
    sleepAgainEvent = t.after(7000, goSleep);
    awake = true;
    wakeup = false;
    showCurrentTime();
    for (int i = 0; i < 7 * 4; i++) {
      segs[i].fillColor(CRGB(255, 0, 15), 1);
    }
  }

  if(awake) { // repeat for x seconds
    showCurrentTime();
  }
  t.update();
  for(int i = 0; i < 7*4; i++) {
    segs[i].draw();
  }
  FastLED.show();
}

