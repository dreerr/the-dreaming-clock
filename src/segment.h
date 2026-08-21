#pragma once
#include <FastLED.h>
#include <stdint.h>

#include "config.h"

// ===========================================================================
// Segment — one 7-segment bar (or the colon), driven by probability.
// ===========================================================================
//
// The display layer does not switch a segment on or off directly. It sets a
// *probability*, and once per animation cycle the segment rolls against it to
// decide whether it lights up for the next cycle. Showing a glyph solidly is
// probability 255; hiding it is 0; everything in between makes the glyph
// flicker into and out of the noise.
//
// The split between tick() and render() matters: tick() is the only thing that
// touches scheduling state, and render() is a pure function of the time elapsed
// since the cycle began. Rendering therefore cannot reset the animation clock,
// which is what used to make crossfades freeze when a caller re-issued a colour
// every frame.

enum class SegmentMode : uint8_t {
  CONSTANT,        // solid colour
  RANDOM_GRADIENT, // drifting random gradient across the bar
  PULSE,           // smooth dim up and down over one cycle
  BLINK,           // hard on for the first half of the cycle, off for the rest
};

const char *segmentModeName(SegmentMode mode);
bool segmentModeFromName(const char *name, SegmentMode &out);

class Segment {
public:
  // --- inputs: written by the display / mode layer ---
  uint8_t probability = 128; // 0 = never lit, 255 = always lit
  uint16_t cycleMs = 5000;   // length of one animation cycle
  uint16_t fadeMs = 2000;    // crossfade into the new cycle's target
  SegmentMode mode = SegmentMode::RANDOM_GRADIENT;
  CRGB color = CRGB::Black;  // CONSTANT / PULSE / BLINK
  uint8_t brightness = 255;  // applied at render time, so it responds at once
  uint8_t hueBase = 0;       // RANDOM_GRADIENT: centre of the hue range
  uint8_t hueSpread = 48;    // RANDOM_GRADIENT: width of the hue range

  void attach(CRGB *strip, int start, int count);

  // Roll a fresh cycle immediately. Use after changing mode or colour when the
  // change should be visible without waiting for the current cycle to end.
  void restart(uint32_t now);

  // Advance to the next cycle if the current one has elapsed.
  void tick(uint32_t now);

  // Write this segment's LEDs for the current instant.
  void render(uint32_t now);

  bool isLit() const { return lit_; }
  bool isAttached() const { return strip_ != nullptr; }

private:
  CRGB *strip_ = nullptr;
  int start_ = 0;
  int count_ = 0;

  CRGB from_[MAX_SEG_LEDS];
  CRGB to_[MAX_SEG_LEDS];
  uint32_t cycleStart_ = 0;
  bool lit_ = false;

  void advance(uint32_t now);
  void snapshotFrom(uint32_t now);
  void buildTarget();
  void fillGradient();
  uint8_t fadeAmount(uint32_t elapsed) const;
};
