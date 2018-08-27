#pragma once
#define SEGMENT_LENGTH 10
#define MIN_SPEED 1
#define MAX_SPEED 6
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
    byte minB = max(0, opacity - gradientRange);
    byte maxB = min(opacity + gradientRange, 255);
    CHSV colorStart = CHSV(random(255), 255, random(minB, maxB));
    CHSV colorMid = CHSV(random(255), 255, random(minB, maxB));
    CHSV colorEnd = CHSV(random(255), 255, random(minB, maxB));
    fill_gradient(hsv_array, 0, colorStart, (numToFill / 2 - 1), colorMid);
    fill_gradient(hsv_array, numToFill / 2, colorMid, numToFill - 1, colorEnd);
    for (int i = 0; i < numToFill; i++) {
      array[i] = hsv_array[i];
    }
  }
  void fillColor(CRGB color, int newSpeed) {
    mode = COLOR;
    newSequence();
    speed = newSpeed;
    color.fadeToBlackBy(255 - opacity);
    for (int i = 0; i < segLength; i++) {
      target[i] = color;
    }
  }

  void newSequence() {
    // Reset Sequence and copy values from live leds
    blendAmount = 0;
    for (int i = 0; i < segLength; i++) {
      current[i] = leds[i + segStart];
    }
  }

  void animationFinished() {
    if(mode == RANDOM) {
      newSequence();
      speed = random(MIN_SPEED, MAX_SPEED);
      fillRandomGradient(target, segLength);
      if (random8(255) > 180) {
        opacity = (random8(255) > 120) ? 255 : 15;
      }
    }
  }

  void drawBlend() {
    if (blendAmount == 255)
      animationFinished();
    for (int i = 0; i < segLength; i++) {
      leds[i + segStart] =
          blend(current[i], target[i], quadwave8(blendAmount / 2));
    }
    blendAmount = min(255, blendAmount + speed);
  }


  void draw() {
    if (initialized) {
      drawBlend();
    }
  }
};
