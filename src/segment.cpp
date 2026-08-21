#include "segment.h"

#include "animation.h"

#include <string.h>

namespace {
const char *const kModeNames[] = {"constant", "pulse",    "blink",
                                  "gradient", "sweep",    "bloom"};
constexpr uint8_t kModeCount = sizeof(kModeNames) / sizeof(kModeNames[0]);
}

const char *segmentModeName(SegmentMode mode) {
  return kModeNames[static_cast<uint8_t>(mode)];
}

bool segmentModeUsesGradient(SegmentMode mode) {
  return mode == SegmentMode::RANDOM_GRADIENT || mode == SegmentMode::SWEEP ||
         mode == SegmentMode::BLOOM;
}

bool segmentModeFromName(const char *name, SegmentMode &out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < kModeCount; i++) {
    if (strcmp(name, kModeNames[i]) == 0) {
      out = static_cast<SegmentMode>(i);
      return true;
    }
  }
  return false;
}

void Segment::attach(CRGB *strip, int start, int count) {
  strip_ = strip;
  start_ = start;
  count_ = count > MAX_SEG_LEDS ? MAX_SEG_LEDS : count;
  memset(from_, 0, sizeof(from_));
  memset(to_, 0, sizeof(to_));
}

void Segment::snapshotFrom(uint32_t now) {
  // Start the new cycle from where this one had got to, so a cycle change never
  // produces a visible jump.
  //
  // This recomputes the current value from from_/to_ rather than reading it
  // back off the strip, because the strip holds brightness-scaled colours and
  // these buffers are kept unscaled. Reading the strip back would fold the old
  // brightness in permanently, and a brightness change would darken the
  // outgoing colour instead of just rescaling it.
  const uint32_t elapsed = now - cycleStart_;

  if (mode == SegmentMode::CONSTANT || mode == SegmentMode::RANDOM_GRADIENT) {
    const uint8_t amount = fadeAmount(elapsed);
    const int32_t offset = mode == SegmentMode::RANDOM_GRADIENT
                               ? driftOffset(phaseAt(elapsed))
                               : 0;
    for (int i = 0; i < count_; i++) {
      const CRGB current = mode == SegmentMode::RANDOM_GRADIENT
                               ? sampleGradient(i, offset)
                               : to_[i];
      from_[i] = blend(from_[i], current, amount);
    }
    return;
  }

  if (mode == SegmentMode::BLINK) {
    const bool on = elapsed * 2 < cycleMs;
    for (int i = 0; i < count_; i++) {
      from_[i] = on ? to_[i] : CRGB(CRGB::Black);
    }
    return;
  }

  // The enveloped modes: capture what the envelope was actually showing, so the
  // next cycle cross-fades out of the real picture rather than snapping from a
  // target the viewer never saw.
  const uint8_t phase = phaseAt(elapsed);
  for (int i = 0; i < count_; i++) {
    CRGB c = to_[i];
    c.nscale8_video(envelopeAt(phase, i));
    from_[i] = c;
  }
}

// Brightness of LED `index` under this mode's envelope at `phase`.
uint8_t Segment::envelopeAt(uint8_t phase, int index) const {
  switch (mode) {
  case SegmentMode::SWEEP:
    return sweepLevel(phase, index, count_);
  case SegmentMode::BLOOM:
    return bloomLevel(phase, index, count_);
  case SegmentMode::PULSE:
    return quadWave(phase);
  default:
    return 255;
  }
}

void Segment::fillGradient() {
  // A *closed* loop of colours: the last stop leads back to the first, so the
  // gradient can slide along the bar indefinitely without a seam crossing it.
  //
  // How many stops and how far apart their hues are is what gives a segment its
  // character — two close stops read as a single colour, five spread ones as a
  // rainbow.
  constexpr uint8_t kStopsMax = 5;
  CHSV stops[kStopsMax];

  uint8_t n = gradientStops;
  if (n < 2) n = 2;
  if (n > kStopsMax) n = kStopsMax;

  // Value varies a little so the gradient reads as depth rather than a flat
  // wash. At full scale — `brightness` is applied when the segment renders.
  constexpr uint8_t kValueSpread = 255 / 4;
  for (uint8_t i = 0; i < n; i++) {
    stops[i] = CHSV(hueBase + random8(hueSpread), 255,
                    random8(255 - kValueSpread, 255));
  }

  for (int i = 0; i < count_; i++) {
    // Position around the closed loop, 8.8 fixed point.
    const uint16_t t = static_cast<uint16_t>(
        (static_cast<uint32_t>(i) * n * 256u) / static_cast<uint32_t>(count_));
    const uint8_t a = static_cast<uint8_t>((t >> 8) % n);
    const uint8_t b = static_cast<uint8_t>((a + 1) % n);
    to_[i] = CRGB(blend(stops[a], stops[b], static_cast<fract8>(t & 0xFF)));
  }
}

namespace {
// Sub-LED resolution for the drift, so a slide across a ten-LED bar is smooth.
constexpr int32_t kDriftSub = 64;
} // namespace

// Sample the closed gradient loop at a sub-LED offset, wrapping around.
CRGB Segment::sampleGradient(int index, int32_t offsetSub) const {
  const int32_t total = static_cast<int32_t>(count_) * kDriftSub;
  int32_t pos = static_cast<int32_t>(index) * kDriftSub + offsetSub;
  pos %= total;
  if (pos < 0) {
    pos += total;
  }
  const int a = static_cast<int>(pos / kDriftSub);
  const int b = (a + 1) % count_;
  const fract8 frac =
      static_cast<fract8>((pos % kDriftSub) * (256 / kDriftSub));
  return blend(to_[a], to_[b], frac);
}

int32_t Segment::driftOffset(uint8_t phase) const {
  if (driftPercent == 0) {
    return 0;
  }
  return (static_cast<int32_t>(phase) * driftPercent * count_ * kDriftSub) /
         (255 * 100);
}

void Segment::buildTarget() {
  if (!lit_) {
    for (int i = 0; i < count_; i++) {
      to_[i] = CRGB::Black;
    }
    return;
  }

  if (segmentModeUsesGradient(mode)) {
    fillGradient();
    return;
  }
  for (int i = 0; i < count_; i++) {
    to_[i] = color;
  }
}

void Segment::advance(uint32_t now) {
  if (!isAttached()) {
    return;
  }
  snapshotFrom(now); // must run before cycleStart_ moves
  cycleStart_ = now;
  cycles_++;

  // The probability roll. random8() yields 0..255, so `random8() < 255` would
  // still fail once every 256 cycles — 255 is special-cased to mean "always"
  // because the clock display relies on it being absolutely steady.
  if (probability >= 255) {
    lit_ = true;
  } else if (probability == 0) {
    lit_ = false;
  } else {
    lit_ = random8() < probability;
  }

  buildTarget();
}

void Segment::restart(uint32_t now) { advance(now); }

void Segment::refreshTarget() {
  if (isAttached()) {
    buildTarget();
  }
}

void Segment::transitionTo(SegmentMode newMode, uint32_t now,
                           uint16_t crossfadeMs) {
  if (!isAttached() || mode == newMode) {
    return;
  }
  snapshotFrom(now); // capture the outgoing picture before anything moves
  mode = newMode;
  cycleStart_ = now;
  fadeMs = crossfadeMs;
  buildTarget(); // lit_ is deliberately untouched
}

void Segment::tick(uint32_t now) {
  if (!isAttached()) {
    return;
  }
  if (now - cycleStart_ >= cycleMs) {
    advance(now);
  }
}

// Position within the cycle, 0..255.
uint8_t Segment::phaseAt(uint32_t elapsed) const {
  if (cycleMs == 0) {
    return 0;
  }
  const uint32_t p = (elapsed * 255UL) / cycleMs;
  return p >= 255 ? 255 : static_cast<uint8_t>(p);
}

uint8_t Segment::fadeAmount(uint32_t elapsed) const {
  if (fadeMs == 0) {
    return 255;
  }
  const uint32_t amount = (elapsed * 255UL) / fadeMs;
  return amount >= 255 ? 255 : static_cast<uint8_t>(amount);
}

void Segment::render(uint32_t now) {
  if (!isAttached()) {
    return;
  }

  const uint32_t elapsed = now - cycleStart_;

  switch (mode) {
  case SegmentMode::CONSTANT: {
    const uint8_t amount = fadeAmount(elapsed);
    for (int i = 0; i < count_; i++) {
      CRGB c = blend(from_[i], to_[i], amount);
      c.nscale8_video(brightness);
      strip_[start_ + i] = c;
    }
    break;
  }

  // The gradient slides along the bar as the cycle runs, so the majority mode
  // is no longer a picture that simply sits there for fifteen seconds.
  case SegmentMode::RANDOM_GRADIENT: {
    const uint8_t amount = fadeAmount(elapsed);
    const int32_t offset = driftOffset(phaseAt(elapsed));
    for (int i = 0; i < count_; i++) {
      CRGB c = blend(from_[i], sampleGradient(i, offset), amount);
      c.nscale8_video(brightness);
      strip_[start_ + i] = c;
    }
    break;
  }

  // The enveloped modes. Each paints its target through a moving envelope and
  // cross-fades in from whatever the previous cycle was showing, so switching
  // animation mid-stream is a hand-over rather than a cut.
  case SegmentMode::PULSE:
  case SegmentMode::SWEEP:
  case SegmentMode::BLOOM: {
    const uint8_t phase = phaseAt(elapsed);
    const uint8_t entry = fadeAmount(elapsed);
    for (int i = 0; i < count_; i++) {
      CRGB target = to_[i];
      target.nscale8_video(envelopeAt(phase, i));
      CRGB c = blend(from_[i], target, entry);
      c.nscale8_video(brightness);
      strip_[start_ + i] = c;
    }
    break;
  }

  case SegmentMode::BLINK: {
    const bool on = lit_ && (elapsed * 2 < cycleMs);
    CRGB c = CRGB::Black;
    if (on) {
      c = color;
      c.nscale8_video(brightness);
    }
    for (int i = 0; i < count_; i++) {
      strip_[start_ + i] = c;
    }
    break;
  }
  }
}
