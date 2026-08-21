#pragma once
#include <stdint.h>

#include "config.h"
#include "segment.h"

// ===========================================================================
// Display — puts glyphs on the segment array.
// ===========================================================================
//
// These set a segment's *probability*, not its brightness. 255 means the
// segment is certain to be lit (a solid, readable glyph); 0 means it is
// certain to be dark; values in between make the glyph flicker into and out of
// whatever the segments are already doing.

// Set one character at digit position 0..NUM_DIGITS-1.
void setChar(int position, char c, uint8_t probability);

// Set a 4-character word. Shorter words are padded with blanks.
void setWord(const char *word, uint8_t probability);

// Set a 4-digit number (e.g. 1435 for 14:35). Values are taken modulo 10000.
void setNumber(int value, uint8_t probability);

// Same as setWord(), but segments outside the glyph keep a background
// probability instead of going dark. This is what lets a word condense out of
// the dream noise rather than replacing it.
void setWordOverNoise(const char *word, uint8_t wordProbability,
                      uint8_t noiseProbability);

// Apply one probability to every digit segment (the colon is left alone).
void setAllDigits(uint8_t probability);

// Apply a mode, colour and timings to every segment including the colon.
void setAllMode(SegmentMode mode, uint16_t cycleMs, uint16_t fadeMs);
void setAllColor(const CRGB &color, uint8_t brightness);
void setAllHue(uint8_t hueBase, uint8_t hueSpread);
void restartAll(uint32_t now);
