#pragma once
#include <stdint.h>

// ===========================================================================
// How a segment paints itself
// ===========================================================================
//
// Kept apart from segment.h, which pulls in FastLED, so that anything wanting
// to name a fill — the message queue, the API — can do so without the LED
// driver coming with it.
//
// Two families. The colour modes paint the flat `color`; the gradient modes
// paint a fresh random gradient and differ only in the spatial envelope they
// move across it.

enum class SegmentMode : uint8_t {
  // Colour modes — flat `color`.
  CONSTANT, // solid
  PULSE,    // dims up and down over the cycle
  BLINK,    // hard on for the first half of the cycle, off for the rest
  // Gradient modes — a random gradient across the bar.
  RANDOM_GRADIENT, // held for the cycle
  SWEEP,           // a lit head runs end to end and back
  BLOOM,           // a band grows from the centre out to both ends and back
};

const char *segmentModeName(SegmentMode mode);
bool segmentModeFromName(const char *name, SegmentMode &out);

// True for the modes that paint a gradient rather than the flat `color`.
bool segmentModeUsesGradient(SegmentMode mode);
