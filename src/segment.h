#pragma once
#define SEGMENT_LENGTH 10
#define MIN_DURATION_MS 1000  // Fastest animation: 1 seconds
#define MAX_DURATION_MS 10000 // Slowest animation: 10 seconds

#include <Arduino.h>
#include <FastLED.h>
#include <math.h>
using namespace std;

enum SegmentMode {
  RANDOM,
  COLOR,
};

class Segment {
private:
  bool initialized = false;
  int segStart;
  int segLength;
  CRGB *leds;
  CRGB *current;
  CRGB *target;
  unsigned long animationStartMs = 0;
  unsigned long nextSequenceStart = 0;

public:
  unsigned int durationMs = 5000; // Animation duration in milliseconds
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
    nextSequenceStart = millis() + random(20000); // refactor this!
    animationStartMs = millis();
    for (int i = 0; i < segLength; i++) {
      current[i] = leds[i + segStart];
    }
  }

  void fillColor(CRGB color, unsigned int newDurationMs) {
    newSequence();
    mode = COLOR;
    durationMs = newDurationMs;
    color.fadeToBlackBy(255 - opacity);
    for (int i = 0; i < segLength; i++) {
      target[i] = color;
      if (newDurationMs == 0) {
        current[i] = color; // Instant change
      }
    }
  }

  void drawBlend() {
    // Calculate blend amount based on elapsed time
    unsigned long elapsed = millis() - animationStartMs;
    int blendAmount =
        (durationMs > 0) ? min(255UL, (elapsed * 255UL) / durationMs) : 255;

    for (int i = 0; i < segLength; i++) {
      leds[i + segStart] = blend(current[i], target[i], blendAmount);
    }

    if (blendAmount >= 255) {
      if (mode == RANDOM && millis() > nextSequenceStart) {
        newSequence();
        durationMs = random(MIN_DURATION_MS, MAX_DURATION_MS);

        // Should be made more elegant
        gradientRange = random(0, 50);
        if (random8(255) > 200) {
          opacity = (random8(255) > 120) ? 255 : 0;
        }

        fillRandomGradient(target, segLength);
      }
    }
  }

  void draw() {
    if (initialized) {
      drawBlend();
    }
  }
};
