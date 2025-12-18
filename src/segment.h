#pragma once
#define SEGMENT_LENGTH 10
#define MIN_SPEED 1
#define MAX_SPEED 4
#include "definitions.h"
#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
using namespace std;

enum SegmentMode { RANDOM, COLOR };

class Segment {
private:
  bool initialized = false;
  int segStart;
  int segLength;
  CRGB *leds;
  CRGB *current;
  CRGB *target;
  int blendAmount = 0;
  unsigned long nextMillis = millis();

public:
  int speed = 255;
  int opacity = 0;
  int gradientRange = 0;
  SegmentMode mode = RANDOM;
  Segment() {}
  Segment(CRGB *leds, int start, int length)
      : leds(leds), segStart(start), segLength(length) {
    initialized = true;
    current = new CRGB[segLength];
    target = new CRGB[segLength];
  }

  void fillRandomGradient(CRGB *array, int numToFill) {
    mode = RANDOM;
    CHSV hsv_array[numToFill];
    int minB = max(0, opacity - gradientRange);
    int maxB = min(opacity + gradientRange, 255);
    int hueDeviation = sin(2 * PI * millis() / 1000.0);
    int hueMin = (millis() / 7803) % 255;
    int hueMax = (millis() / 1000) % 255;
    CHSV colorStart = CHSV(random(hueMin, hueMax), 255, random(minB, maxB));
    CHSV colorMid = CHSV(random(hueMin, hueMax), 255, random(minB, maxB));
    CHSV colorEnd = CHSV(random(hueMin, hueMax), 255, random(minB, maxB));
    fill_gradient(hsv_array, 0, colorStart, (numToFill / 2 - 1), colorMid);
    fill_gradient(hsv_array, numToFill / 2, colorMid, numToFill - 1, colorEnd);
    for (int i = 0; i < numToFill; i++) {
      array[i] = hsv_array[i];
    }
  }

  void newSequence() {
    // Reset Sequence and copy values from live leds
    blendAmount = 0;
    nextMillis = millis() + random(10000);
    for (int i = 0; i < segLength; i++) {
      current[i] = leds[i + segStart];
    }
  }

  void fillColor(CRGB color, int newSpeed) {
    mode = COLOR;
    newSequence();
    speed = newSpeed;
    color.fadeToBlackBy(255 - opacity);
    for (int i = 0; i < segLength; i++) {
      target[i] = color;
      if (speed == 255) {
        current[i] = color;
      }
    }
  }

  void animationFinished() {
    if (mode == RANDOM && nextMillis < millis()) {
      newSequence();
      speed = random(MIN_SPEED, MAX_SPEED);
      // gradientRange = random(0, 50);
      // opacity = 0;
      if (random8(255) > 200) {
        opacity = (random8(255) > 120) ? 255 : 0;
      }
      fillRandomGradient(target, segLength);
    }
  }

  void drawBlend() {
    if (blendAmount == 255) {
      animationFinished();
    }
    for (int i = 0; i < segLength; i++) {
      leds[i + segStart] =
          blend(current[i], target[i], quadwave8(blendAmount / 2));
      // blend(current[i], target[i], blendAmount);
    }
    blendAmount = min(255, blendAmount + speed);
  }

  void draw() {
    if (initialized) {
      drawBlend();
    }
  }
};
