#include "leds.h"

#include <Arduino.h>

#include "modes.h"
#include "settings.h"

CRGB leds[NUM_LEDS];
Segment segments[NUM_SEGMENTS];

namespace {
uint32_t lastFrameMs = 0;
bool blanked = false;
constexpr uint32_t kFrameIntervalMs = 1000 / FRAMES_PER_SECOND;

// Calibration walks a single lit LED along the strip so the physical order of
// the LEDs inside each segment can be read off and compared with the web
// preview. The SVG geometry alone cannot say which end of a bar is LED 0.
bool calibrating = false;
int calibrationLed = 0;
uint32_t lastCalibrationStepMs = 0;
constexpr uint32_t kCalibrationStepMs = 600;

void renderCalibration(uint32_t now) {
  if (now - lastCalibrationStepMs >= kCalibrationStepMs) {
    lastCalibrationStepMs = now;
    calibrationLed = (calibrationLed + 1) % NUM_LEDS;
  }
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  leds[calibrationLed] = CRGB::White;
}
} // namespace

void setupLEDs() {
  Serial.println(F("=== LEDs ==="));
  Serial.printf("  APA102, %d LEDs, %d segments, data GPIO%d / clock GPIO%d\n",
                NUM_LEDS, NUM_SEGMENTS, LED_DATA_PIN, LED_CLOCK_PIN);

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].attach(leds, segmentLedStart(i), segmentLedCount(i));
  }

  FastLED.addLeds<APA102, LED_DATA_PIN, LED_CLOCK_PIN, BGR>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.showColor(CRGB::Black);

  setupModes(millis());
  Serial.println(F("============\n"));
}

void setCalibration(bool on) {
  calibrating = on;
  calibrationLed = 0;
  blanked = false;
  Serial.printf("[LED] Calibration %s\n", on ? "on" : "off");
}

bool calibrationActive() { return calibrating; }

int calibrationLedIndex() { return calibrationLed; }

void loopLEDs() {
  const uint32_t now = millis();
  if (now - lastFrameMs < kFrameIntervalMs) {
    return;
  }
  lastFrameMs = now;

  // Always runs, including while the display is off, so the wakeup and dream
  // timers keep advancing.
  updateMode(now);

  if (calibrating) {
    renderCalibration(now);
    FastLED.show();
    return;
  }

  if (currentDisplayMode() == DisplayMode::OFF) {
    if (!blanked) {
      // Clear the buffer rather than only the strip, so the web preview shows
      // the same thing the clock is showing.
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      FastLED.show();
      blanked = true;
    }
    return;
  }
  blanked = false;

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].tick(now);
    segments[i].render(now);
  }
  FastLED.show();
}
