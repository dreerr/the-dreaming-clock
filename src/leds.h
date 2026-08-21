#pragma once
#include <FastLED.h>

#include "config.h"
#include "segment.h"

// The LED strip and the segments that draw into it. Defined once, in leds.cpp.
extern CRGB leds[NUM_LEDS];
extern Segment segments[NUM_SEGMENTS];

void setupLEDs();
void loopLEDs();

// Walks one lit LED along the strip so the physical LED order inside each
// segment can be matched against the web preview.
void setCalibration(bool on);
bool calibrationActive();
int calibrationLedIndex();
