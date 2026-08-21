#include "display.h"

#include "leds.h"
#include "patterns.h"

namespace {

// Set the seven segments of one digit from a glyph bit pattern.
void applyGlyph(int position, uint8_t glyph, uint8_t onProbability,
                uint8_t offProbability) {
  const int base = position * SEGMENTS_PER_DIGIT;
  for (int s = 0; s < SEGMENTS_PER_DIGIT; s++) {
    const bool on = (glyph >> s) & 0x01;
    segments[base + s].probability = on ? onProbability : offProbability;
  }
}

} // namespace

void setChar(int position, char c, uint8_t probability) {
  if (position < 0 || position >= NUM_DIGITS) {
    return;
  }
  applyGlyph(position, glyphFor(c), probability, 0);
}

void setWord(const char *word, uint8_t probability) {
  setWordOverNoise(word, probability, 0);
}

void setWordOverNoise(const char *word, uint8_t wordProbability,
                      uint8_t noiseProbability) {
  if (word == nullptr) {
    return;
  }
  for (int i = 0; i < NUM_DIGITS; i++) {
    const char c = word[i]; // relies on the caller passing a 4-char word
    if (c == '\0') {
      applyGlyph(i, 0, wordProbability, noiseProbability);
      continue;
    }
    applyGlyph(i, glyphFor(c), wordProbability, noiseProbability);
  }
}

void setNumber(int value, uint8_t probability) {
  int v = value % 10000;
  if (v < 0) {
    v = 0;
  }
  // Integer digit split — the old version called pow() four times per frame on
  // a core without an FPU.
  setChar(0, static_cast<char>('0' + (v / 1000)), probability);
  setChar(1, static_cast<char>('0' + ((v / 100) % 10)), probability);
  setChar(2, static_cast<char>('0' + ((v / 10) % 10)), probability);
  setChar(3, static_cast<char>('0' + (v % 10)), probability);
}

void setAllDigits(uint8_t probability) {
  for (int i = 0; i < NUM_DIGIT_SEGMENTS; i++) {
    segments[i].probability = probability;
  }
}

void setAllMode(SegmentMode mode, uint16_t cycleMs, uint16_t fadeMs) {
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].mode = mode;
    segments[i].cycleMs = cycleMs;
    segments[i].fadeMs = fadeMs;
  }
}

void setAllColor(const CRGB &color, uint8_t brightness) {
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].color = color;
    segments[i].brightness = brightness;
  }
}

void setAllHue(uint8_t hueBase, uint8_t hueSpread) {
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].hueBase = hueBase;
    segments[i].hueSpread = hueSpread;
  }
}

void restartAll(uint32_t now) {
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].restart(now);
  }
}
