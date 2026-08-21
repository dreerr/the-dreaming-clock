#include "patterns.h"

namespace {

// Digits 0-9.
const uint8_t kDigits[10] = {
    0x77, // 0
    0x44, // 1
    0x6B, // 2
    0x6E, // 3
    0x5C, // 4
    0x3E, // 5
    0x3F, // 6
    0x64, // 7
    0x7F, // 8
    0x7E, // 9
};

// Letters A-Z. A 7-segment cell cannot render every letter, so several are
// approximations and several are unavoidably identical to a digit or to each
// other. The shape actually drawn is noted in the comment.
const uint8_t kLetters[26] = {
    0x7D, // A
    0x1F, // b  (lowercase shape; 'B' is indistinguishable from '8')
    0x33, // C
    0x4F, // d  (lowercase shape)
    0x3B, // E
    0x39, // F
    0x37, // G
    0x5D, // H
    0x11, // I  (left verticals)
    0x47, // J
    0x5D, // K  — approximated as H
    0x13, // L
    0x75, // M  — approximation
    0x15, // n  (lowercase shape)
    0x77, // O  — identical to '0'
    0x79, // P
    0x7C, // q  (lowercase shape)
    0x11, // r  — approximated as I
    0x3E, // S  — identical to '5'
    0x1B, // t  (lowercase shape)
    0x57, // U
    0x57, // V  — approximated as U
    0x57, // W  — approximated as U
    0x5D, // X  — approximated as H
    0x5E, // y  (lowercase shape)
    0x6B, // Z  — identical to '2'
};

} // namespace

uint8_t glyphFor(char c) {
  if (c >= '0' && c <= '9') {
    return kDigits[c - '0'];
  }
  if (c >= 'A' && c <= 'Z') {
    return kLetters[c - 'A'];
  }
  if (c >= 'a' && c <= 'z') {
    return kLetters[c - 'a'];
  }
  return 0; // space and everything else: all segments off
}

bool isRenderable(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z');
}
