#pragma once
#include <stdint.h>

// ===========================================================================
// Dream words
// ===========================================================================
//
// Words shown during dream mode. Arduino-free so the list can be validated on
// the host (every word must be exactly NUM_DIGITS characters and renderable on
// a 7-segment cell).
//
// A 7-segment cell has one glyph per letter, so case is NOT distinguishable —
// "bEEp" and "BEEP" render identically. The mixed case kept below is a note to
// the reader about which shape the glyph table draws, nothing more.

constexpr int DREAM_WORD_LENGTH = 4;

int dreamWordCount();
const char *dreamWordAt(int index);

// The next word to show. This is the seam an external source would replace:
// swapping the built-in list for HTTP- or MQTT-delivered words means changing
// this one function, not the mode layer.
const char *nextDreamWord();

void seedDreamWords(uint32_t seed);
