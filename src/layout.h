#pragma once

// ===========================================================================
// Segment -> LED-strip mapping
// ===========================================================================
//
// The physical strip runs straight through the clock face, and the colon sits
// in the middle of it: digit segments before the colon are at their natural
// offset, everything after is shifted by the colon's LEDs.
//
// The mapping is parameterised rather than hard-coded so a host test can sweep
// every LED count in one binary. src/config.h binds these to the configured
// values as segmentLedStart() / segmentLedCount(); callers use those.
//
// Arduino-free and constexpr on purpose: the firmware evaluates it at compile
// time, and the tests link it directly.

// Number of digit segments that come before the colon. The colon splits the
// display in half, so this is simply half of them.
constexpr int digitSegmentsBeforeColon(int digitSegments) {
  return digitSegments / 2;
}

// First LED index of the colon.
constexpr int colonLedStartFor(int digitSegments, int ledsPerSegment) {
  return digitSegmentsBeforeColon(digitSegments) * ledsPerSegment;
}

// Total LEDs on the strip.
constexpr int numLedsFor(int digitSegments, int ledsPerSegment, int colonLeds) {
  return digitSegments * ledsPerSegment + colonLeds;
}

// First LED index of a segment. `seg` in [0, digitSegments] where the last
// index is the colon.
constexpr int segmentLedStartFor(int seg, int digitSegments, int ledsPerSegment,
                                 int colonLeds) {
  return seg >= digitSegments
             ? colonLedStartFor(digitSegments, ledsPerSegment)
             : (seg >= digitSegmentsBeforeColon(digitSegments)
                    ? seg * ledsPerSegment + colonLeds
                    : seg * ledsPerSegment);
}

// LEDs belonging to a segment.
constexpr int segmentLedCountFor(int seg, int digitSegments, int ledsPerSegment,
                                 int colonLeds) {
  return seg >= digitSegments ? colonLeds : ledsPerSegment;
}
