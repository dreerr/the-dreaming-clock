#pragma once
#include <Arduino.h>

/*
 * 7-Segment layout of a digit:
 *
 *       ┌───5───┐
 *       │       │
 *       4       6
 *       │       │
 *       ├───3───┤
 *       │       │
 *       0       2
 *       │       │
 *       └───1───┘
 *
 * Bit order: [6][5][4][3][2][1][0]
 */

// Combined 7-segment patterns for digits and letters
const uint8_t segmentPatterns[] = {
    // Digits 0-9 (index 0-9)
    0x77, // 0: segments 0,1,2,4,5,6
    0x44, // 1: segments 2,6
    0x6B, // 2: segments 0,1,3,5,6
    0x6E, // 3: segments 1,2,3,5,6
    0x5C, // 4: segments 2,3,4,6
    0x3E, // 5: segments 1,2,3,4,5
    0x3F, // 6: segments 0,1,2,3,4,5
    0x64, // 7: segments 2,5,6
    0x7F, // 8: segments 0,1,2,3,4,5,6
    0x7E, // 9: segments 1,2,3,4,5,6
    // Letters A-Z (index 10-35)
    0x7D, // A: segments 0,2,3,4,5,6
    0x1F, // b: segments 0,1,2,3,4
    0x33, // C: segments 0,1,4,5
    0x4F, // d: segments 0,1,2,3,6
    0x3B, // E: segments 0,1,3,4,5
    0x39, // F: segments 0,3,4,5
    0x37, // G: segments 0,1,2,4,5
    0x5D, // H: segments 0,2,3,4,6
    0x11, // I: segments 0,4
    0x47, // J: segments 0,1,2,6
    0x5D, // K: (same as H)
    0x13, // L: segments 0,1,4
    0x75, // M: segments 0,2,4,5,6 (approximation)
    0x15, // n: segments 0,2,4
    0x77, // O: (same as 0)
    0x79, // P: segments 0,3,4,5,6
    0x7C, // q: segments 2,3,4,5,6
    0x11, // r: segments 0,4
    0x3E, // S: (same as 5)
    0x1B, // t: segments 0,1,3,4
    0x57, // U: segments 0,1,2,4,6
    0x57, // V: (same as U)
    0x57, // W: (same as U, approximation)
    0x5D, // X: (same as H)
    0x5E, // y: segments 1,2,3,4,6
    0x6B, // Z: (same as 2)
};

// Get pattern index for a character
inline int8_t getPatternIndex(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'Z') {
    return 10 + (c - 'A');
  } else if (c >= 'a' && c <= 'z') {
    return 10 + (c - 'a');
  }
  return -1; // Unknown character
}
