#pragma once
#include <Arduino.h>
#include <FastLED.h>
#include <RTClib.h>

#include "patterns.h"
#include "segment.h"

// External references
extern Segment segments[];
extern CHSV mainColor;
DateTime getCurrentTime();

// Set a character (0-9, A-Z, a-z) at a position with given opacity
// Note: This only sets opacity - caller must set color/mode separately
inline void setChar(int position, char c, int opacity) {
  int8_t patternIndex = getPatternIndex(c);
  int start = position * 7;

  if (patternIndex < 0) {
    // Unknown character - turn off all segments
    for (int segPos = 0; segPos < 7; segPos++) {
      segments[start + segPos].opacity = 0;
    }
    return;
  }

  uint8_t pattern = segmentPatterns[patternIndex];
  for (int segPos = 0; segPos < 7; segPos++) {
    if ((pattern >> segPos) & 0x01) {
      segments[start + segPos].opacity = opacity;
    } else {
      segments[start + segPos].opacity = 0;
    }
  }
}

// Set a digit (0-9) at a position - convenience wrapper
inline void setDigit(int position, int value, int opacity) {
  if (value >= 0 && value <= 9) {
    setChar(position, '0' + value, opacity);
  }
}

// Set a 4-digit number on the display
inline void setNumber(int value, int opacity) {
  const int numDigits = 4;
  for (int i = 0; i < numDigits; i++) {
    setDigit(i, value / ((int)pow(10, numDigits - 1 - i)) % 10, opacity);
  }
}

// Display the current time from RTC
inline void showCurrentTime() {
  DateTime now = getCurrentTime();
  int timeValue = now.minute() + now.hour() * 100;

  // Set the time digits
  setNumber(timeValue, 255);

  // // Apply color to all digit segments that are on
  for (int i = 0; i < 7 * 4; i++) {
    segments[i].mode = SegmentMode::COLOR;
  }

  // Blinking colon
  bool colonOn = ((millis() % 2000) > 1000);
  segments[COLON_INDEX].opacity = colonOn ? 255 : 0;
  segments[COLON_INDEX].fillColor(mainColor, 255);
}
