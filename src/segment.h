#pragma once
#include <FastLED.h>
#include <stdint.h>

#include "config.h"
#include "segmentmode.h"

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

class Segment {
public:
  // --- inputs: written by the display / mode layer ---
  uint8_t probability = 128; // 0 = never lit, 255 = always lit
  uint16_t cycleMs = 5000;   // length of one animation cycle
  uint16_t fadeMs = 2000;    // crossfade into the new cycle's target
  SegmentMode mode = SegmentMode::RANDOM_GRADIENT;
  CRGB color = CRGB::Black;  // CONSTANT / PULSE / BLINK
  uint8_t brightness = 255;  // applied at render time, so it responds at once
  // The gradient modes build a closed loop of `gradientStops` colours picked
  // within `hueSpread` of `hueBase`. Those two give a segment its character:
  // a narrow spread with few stops reads as one colour, a wide spread with
  // many reads as a rainbow — and each segment picks its own.
  uint8_t hueBase = 0;
  uint8_t hueSpread = 48;
  uint8_t gradientStops = 3; // 2..5 distinct colours around the loop

  // How far the gradient slides along the bar over one cycle, as a signed
  // percentage of the bar's length. The loop is closed, so it can drift
  // forever without a seam.
  int8_t driftPercent = 0;

  void attach(CRGB *strip, int start, int count);

  // Roll a fresh cycle immediately. Use after changing mode or colour when the
  // change should be visible without waiting for the current cycle to end.
  void restart(uint32_t now);

  // Change animation mid-cycle, cross-fading out of whatever is on screen.
  // Unlike restart() this keeps the probability roll, so settling a segment
  // cannot switch it off underneath a word.
  void transitionTo(SegmentMode newMode, uint32_t now, uint16_t crossfadeMs);

  // Advance to the next cycle if the current one has elapsed.
  void tick(uint32_t now);

  // Write this segment's LEDs for the current instant.
  void render(uint32_t now);

  // Rebuild the target from the current palette without disturbing the cycle.
  // Needed because the target is built when a cycle begins, which is a frame
  // before the mode layer gets to choose that cycle's colours.
  void refreshTarget();

  bool isLit() const { return lit_; }

  // Increments on every new cycle. The mode layer watches this to re-roll a
  // segment's animation exactly when it starts a fresh one, rather than
  // re-deciding underneath it 60 times a second.
  uint16_t cycles() const { return cycles_; }
  bool isAttached() const { return strip_ != nullptr; }

private:
  CRGB *strip_ = nullptr;
  int start_ = 0;
  int count_ = 0;

  CRGB from_[MAX_SEG_LEDS];
  CRGB to_[MAX_SEG_LEDS];
  uint32_t cycleStart_ = 0;
  uint16_t cycles_ = 0;
  bool lit_ = false;

  void advance(uint32_t now);
  void snapshotFrom(uint32_t now);
  void buildTarget();
  void fillGradient();
  uint8_t fadeAmount(uint32_t elapsed) const;
  uint8_t phaseAt(uint32_t elapsed) const;
  uint8_t envelopeAt(uint8_t phase, int index) const;
  CRGB sampleGradient(int index, int32_t offsetSub) const;
  int32_t driftOffset(uint8_t phase) const;
};
