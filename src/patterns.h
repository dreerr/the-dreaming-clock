#pragma once
#include <stdint.h>

// 7-segment glyph table. Deliberately free of Arduino headers so it can be
// unit-tested on the host (see test/test_logic).
//
//       ┌───5───┐
//       │       │
//       4       6
//       │       │
//       ├───3───┤
//       │       │
//       0       2
//       │       │
//       └───1───┘
//
// Bit order: [6][5][4][3][2][1][0]

constexpr uint8_t SEG_A = 1 << 0; // lower left
constexpr uint8_t SEG_B = 1 << 1; // bottom
constexpr uint8_t SEG_C = 1 << 2; // lower right
constexpr uint8_t SEG_D = 1 << 3; // middle
constexpr uint8_t SEG_E = 1 << 4; // upper left
constexpr uint8_t SEG_F = 1 << 5; // top
constexpr uint8_t SEG_G = 1 << 6; // upper right

// Returns the 7-segment bit pattern for a character, or 0 (all segments off)
// for anything it cannot render — including space.
//
// NOTE: a 7-segment cell has one glyph per letter. Case is NOT distinguishable:
// 'b' and 'B' return the same pattern, as do 'o'/'O' and so on. Mixed case in
// word lists is purely a hint to the reader about which shape was intended.
uint8_t glyphFor(char c);

// True if the character has a renderable glyph (digits and A-Z/a-z).
bool isRenderable(char c);
