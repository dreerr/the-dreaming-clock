#pragma once
#include <stdint.h>

#include "layout.h"

// ===========================================================================
// Compile-time hardware and identity configuration.
// ===========================================================================

#define AP_SSID "the dreaming clock"
#define HOSTNAME "the-dreaming-clock"

// OTA password comes from the CLOCK_OTA_PASSWORD environment variable at build
// time (see platformio.ini / README). If it is unset the macro is defined as an
// empty string, which setupOTA() detects and warns about.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

#define SETTINGS_NAMESPACE "clock-settings"

// ---------------------------------------------------------------------------
// LED strip
// ---------------------------------------------------------------------------
constexpr int LED_DATA_PIN = 6;  // GPIO6 on ESP32-C3
constexpr int LED_CLOCK_PIN = 7; // GPIO7 on ESP32-C3
constexpr int FRAMES_PER_SECOND = 60;

// ---- The one knob -------------------------------------------------------
// How many LEDs sit inside each digit bar, and inside each colon dot. Change
// these and rebuild; everything below is derived, the static_asserts re-check
// the mapping, and the web preview picks the new layout up from /api/layout
// without needing to be regenerated.
constexpr int LEDS_PER_SEGMENT = 10;
constexpr int COLON_LEDS = 2;
// -------------------------------------------------------------------------

constexpr int NUM_DIGITS = 4;
constexpr int SEGMENTS_PER_DIGIT = 7;
constexpr int NUM_DIGIT_SEGMENTS = NUM_DIGITS * SEGMENTS_PER_DIGIT; // 28
constexpr int COLON_INDEX = NUM_DIGIT_SEGMENTS;                     // 28
constexpr int NUM_SEGMENTS = NUM_DIGIT_SEGMENTS + 1;                // 29

constexpr int NUM_LEDS =
    numLedsFor(NUM_DIGIT_SEGMENTS, LEDS_PER_SEGMENT, COLON_LEDS);
constexpr int COLON_LED_START =
    colonLedStartFor(NUM_DIGIT_SEGMENTS, LEDS_PER_SEGMENT);

// Sizes the per-segment buffers in Segment, so it has to cover the largest
// segment — which is not necessarily a digit bar.
constexpr int MAX_SEG_LEDS =
    LEDS_PER_SEGMENT > COLON_LEDS ? LEDS_PER_SEGMENT : COLON_LEDS;

// The single source of truth for segment -> LED-strip mapping. The web preview
// reads the results of these over /api/layout rather than reimplementing them.
constexpr int segmentLedStart(int seg) {
  return segmentLedStartFor(seg, NUM_DIGIT_SEGMENTS, LEDS_PER_SEGMENT,
                            COLON_LEDS);
}

constexpr int segmentLedCount(int seg) {
  return segmentLedCountFor(seg, NUM_DIGIT_SEGMENTS, LEDS_PER_SEGMENT,
                            COLON_LEDS);
}

// Segments whose LEDs are wired opposite to the direction the artwork runs.
// Measured with the calibration walk: within every digit, positions 0 (lower
// left), 3 (middle) and 6 (upper right) fill backwards. The colon is never
// reversed.
//
// This is a wiring fact, so it lives here and is published at /api/layout — the
// web preview flips those bars rather than carrying its own copy of the list.
//
// The renderer itself does not compensate: every current segment mode is either
// a solid colour or a random gradient, so the physical direction is invisible on
// the clock. A future direction-dependent effect (a wipe, a sweep) would need
// Segment to honour this too.
constexpr bool segmentIsReversed(int seg) {
  return seg < NUM_DIGIT_SEGMENTS &&
         (seg % SEGMENTS_PER_DIGIT == 0 || seg % SEGMENTS_PER_DIGIT == 3 ||
          seg % SEGMENTS_PER_DIGIT == 6);
}

// These hold for any LED count, so they check the mapping rather than one
// arithmetic result.
static_assert(LEDS_PER_SEGMENT >= 1 && COLON_LEDS >= 1,
              "every segment needs at least one LED");
static_assert(NUM_DIGIT_SEGMENTS % 2 == 0,
              "the colon splits the digits in half");
static_assert(segmentLedStart(0) == 0, "the strip starts at the first segment");
static_assert(segmentLedStart(NUM_DIGIT_SEGMENTS / 2 - 1) + LEDS_PER_SEGMENT ==
                  COLON_LED_START,
              "the last segment before the colon must end where it begins");
static_assert(segmentLedStart(NUM_DIGIT_SEGMENTS / 2) ==
                  COLON_LED_START + COLON_LEDS,
              "the first segment after the colon must start past it");
static_assert(segmentLedStart(COLON_INDEX) + COLON_LEDS <= NUM_LEDS,
              "the colon must fit on the strip");
static_assert(segmentLedStart(NUM_DIGIT_SEGMENTS - 1) + LEDS_PER_SEGMENT ==
                  NUM_LEDS,
              "segment mapping must cover the whole strip exactly");

// ---------------------------------------------------------------------------
// RTC (I2C)
// ---------------------------------------------------------------------------
constexpr int I2C_SDA_PIN = 4;
constexpr int I2C_SCL_PIN = 5;

// ---------------------------------------------------------------------------
// Web preview
// ---------------------------------------------------------------------------
constexpr uint32_t PREVIEW_INTERVAL_MS = 40; // 25 FPS
