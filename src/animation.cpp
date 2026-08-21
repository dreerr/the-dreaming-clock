#include "animation.h"

namespace {

// Sixteenths of an LED.
constexpr int kSub = 16;
// The head is a soft comet, not a dot: full brightness within kSweepCore of
// the centre, fading to nothing by kSweepTail. Roughly four LEDs are lit at
// once on a ten-LED bar.
constexpr int kSweepCore = 1 * kSub;
constexpr int kSweepTail = 4 * kSub;

constexpr int kBloomEdge = 2 * kSub; // the band's boundary is ~2 LEDs soft

int absInt(int v) { return v < 0 ? -v : v; }

} // namespace

uint8_t triWave(uint8_t phase) {
  const uint16_t p = phase < 128 ? phase : 255 - phase; // 0..127
  return static_cast<uint8_t>((p * 255) / 127);
}

uint8_t quadWave(uint8_t phase) {
  const uint16_t t = triWave(phase);
  if (t < 128) {
    return static_cast<uint8_t>((t * t * 2) / 255);
  }
  const uint16_t u = 255 - t;
  return static_cast<uint8_t>(255 - (u * u * 2) / 255);
}

uint8_t dreamHueBase(uint32_t nowMs) {
  return static_cast<uint8_t>((nowMs / 400) & 0xFF);
}

uint8_t dreamHueSpread(uint32_t nowMs) {
  // Note: this restarts at the millis() rollover, ~49 days in. The clock will
  // simply be at a different point in the breath; nothing breaks.
  const uint32_t phase =
      (nowMs % DREAM_COHERENCE_PERIOD_MS) * 256u / DREAM_COHERENCE_PERIOD_MS;
  const uint32_t w = triWave(static_cast<uint8_t>(phase));
  return static_cast<uint8_t>(
      DREAM_SPREAD_MIN +
      (static_cast<uint32_t>(DREAM_SPREAD_MAX - DREAM_SPREAD_MIN) * w) / 255);
}

uint8_t rampUp(uint8_t phase) {
  // The rising half of quadWave, stretched over the whole range.
  return quadWave(static_cast<uint8_t>(phase >> 1));
}

uint8_t sweepLevel(uint8_t phase, int index, int count) {
  if (index < 0 || index >= count) {
    return 0;
  }
  if (count <= 1) {
    return triWave(phase);
  }

  const int span = (count - 1) * kSub;
  const int head = (triWave(phase) * span) / 255;
  const int distance = absInt(index * kSub - head);
  if (distance <= kSweepCore) {
    return 255;
  }
  if (distance >= kSweepTail) {
    return 0;
  }
  const int falloff = kSweepTail - kSweepCore;
  return static_cast<uint8_t>(255 - ((distance - kSweepCore) * 255) / falloff);
}

uint8_t bloomLevel(uint8_t phase, int index, int count) {
  if (index < 0 || index >= count) {
    return 0;
  }
  if (count <= 1) {
    return phase < 128 ? rampUp(static_cast<uint8_t>(phase * 2))
                       : static_cast<uint8_t>(255 - rampUp(static_cast<uint8_t>(
                             (phase - 128) * 2)));
  }

  const int span = (count - 1) * kSub;
  const int centre = span / 2;

  // Two halves: light grows outward, then dark grows outward behind it. The
  // bar therefore wipes clean rather than shrinking back the way it came.
  const bool erasing = phase >= 128;
  const uint8_t half = static_cast<uint8_t>((erasing ? phase - 128 : phase) * 2);

  // The radius starts a full edge-width *inside* nothing and overshoots by the
  // same amount at the end, so each half begins with no effect at all and
  // finishes having covered the bar completely. Without the negative start, the
  // soft edge put a full-depth notch in the middle of the bar the instant the
  // erase began — a visible jump every cycle, measured at up to 148/255.
  const int radius =
      -kBloomEdge + (rampUp(half) * (centre + 2 * kBloomEdge)) / 255;
  const int distance = absInt(index * kSub - centre);

  int level;
  if (distance <= radius) {
    level = 255;
  } else {
    const int beyond = distance - radius;
    level = beyond >= kBloomEdge ? 0 : 255 - (beyond * 255) / kBloomEdge;
  }
  return static_cast<uint8_t>(erasing ? 255 - level : level);
}
