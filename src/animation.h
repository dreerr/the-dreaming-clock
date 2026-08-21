#pragma once
#include <stdint.h>

// ===========================================================================
// Animation maths
// ===========================================================================
//
// Arduino-free on purpose: "does the colour variety actually vary over four
// minutes?" and "does the sweep reach every LED?" are claims that should be
// checked by a test rather than by squinting at the clock.

// 0 -> 255 -> 0 across the phase range, linear.
uint8_t triWave(uint8_t phase);

// 0 -> 255 -> 0, eased at both ends so consecutive cycles join without a
// visible corner. Replaces FastLED's quadwave8 so there is one wave
// implementation in the project rather than two.
uint8_t quadWave(uint8_t phase);

// ---------------------------------------------------------------------------
// Dream palette
// ---------------------------------------------------------------------------
//
// hueSpread is the single lever for how alike the segments look: fillGradient()
// picks every stop as hueBase + random8(hueSpread), so a narrow spread puts the
// whole display in one slice of the wheel and a wide one scatters it. Holding
// it at a constant 48 is what made the clock permanently near-monochrome.

// A full narrow -> wide -> narrow trip.
constexpr uint32_t DREAM_COHERENCE_PERIOD_MS = 240000; // 4 minutes
constexpr uint8_t DREAM_SPREAD_MIN = 12;               // reads as one colour
constexpr uint8_t DREAM_SPREAD_MAX = 255;              // the whole wheel

// Centre of the hue range; one turn of the wheel every ~100 s.
uint8_t dreamHueBase(uint32_t nowMs);

// Width of the hue range, breathing between DREAM_SPREAD_MIN and _MAX.
uint8_t dreamHueSpread(uint32_t nowMs);

// ---------------------------------------------------------------------------
// Spatial envelopes
// ---------------------------------------------------------------------------
//
// Brightness for LED `index` of `count` at `phase` within the cycle. Positions
// are worked out in sixteenths of an LED — a bar is only ten LEDs long, so an
// integer head position would visibly step instead of gliding.

// A monotonic eased ramp, 0 -> 255 across the phase range.
uint8_t rampUp(uint8_t phase);

// A soft comet running end to end and back: a plateau of full brightness around
// the head with a long tail, so several LEDs are lit at once rather than a
// single point darting along the bar.
uint8_t sweepLevel(uint8_t phase, int index, int count);

// Loops in two halves rather than expanding and contracting: light grows from
// the centre out to both ends, then darkness grows from the centre out to both
// ends, wiping it away. Symmetric about the middle of the bar at every phase.
uint8_t bloomLevel(uint8_t phase, int index, int count);
