#pragma once
#include <stdint.h>

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

constexpr int NUM_LEDS = 282;
constexpr int NUM_DIGITS = 4;
constexpr int SEGMENTS_PER_DIGIT = 7;
constexpr int NUM_DIGIT_SEGMENTS = NUM_DIGITS * SEGMENTS_PER_DIGIT; // 28
constexpr int COLON_INDEX = NUM_DIGIT_SEGMENTS;                     // 28
constexpr int NUM_SEGMENTS = NUM_DIGIT_SEGMENTS + 1;                // 29

constexpr int LEDS_PER_SEGMENT = 10;
constexpr int COLON_LEDS = 2;
constexpr int COLON_LED_START = 140;
constexpr int MAX_SEG_LEDS = LEDS_PER_SEGMENT;

// The single source of truth for segment -> LED-strip mapping. The colon sits
// physically in the middle of the strip, so every digit segment after it is
// shifted by COLON_LEDS. Both the renderer and the web preview use these, so
// the offset only exists in one place.
constexpr int segmentLedStart(int seg) {
  return seg == COLON_INDEX
             ? COLON_LED_START
             : (seg * LEDS_PER_SEGMENT >= COLON_LED_START
                    ? seg * LEDS_PER_SEGMENT + COLON_LEDS
                    : seg * LEDS_PER_SEGMENT);
}

constexpr int segmentLedCount(int seg) {
  return seg == COLON_INDEX ? COLON_LEDS : LEDS_PER_SEGMENT;
}

static_assert(segmentLedStart(0) == 0, "first segment starts at 0");
static_assert(segmentLedStart(13) == 130, "last segment before the colon");
static_assert(segmentLedStart(14) == 142, "first segment after the colon");
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
