#include "segment.h"

#include <string.h>

namespace {
const char *const kModeNames[] = {"constant", "gradient", "pulse", "blink"};
}

const char *segmentModeName(SegmentMode mode) {
  return kModeNames[static_cast<uint8_t>(mode)];
}

bool segmentModeFromName(const char *name, SegmentMode &out) {
  if (name == nullptr) {
    return false;
  }
  for (uint8_t i = 0; i < 4; i++) {
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
    for (int i = 0; i < count_; i++) {
      from_[i] = blend(from_[i], to_[i], amount);
    }
    return;
  }
  // PULSE and BLINK render a flat colour; carry it forward as it stands.
  for (int i = 0; i < count_; i++) {
    from_[i] = to_[i];
  }
}

void Segment::fillGradient() {
  CHSV hsv[MAX_SEG_LEDS];

  // Value varies a little across the bar so the gradient reads as depth rather
  // than a flat wash. This is at full scale — `brightness` is applied when the
  // segment renders, not when its target is built, so changing it is visible
  // immediately instead of waiting for the next cycle.
  constexpr uint8_t kSpread = 255 / 4;

  auto pick = [&]() {
    return CHSV(hueBase + random8(hueSpread), 255,
                random8(255 - kSpread, 255));
  };

  CHSV start = pick();
  CHSV mid = pick();
  CHSV end = pick();

  if (count_ <= 2) {
    // The colon is only two LEDs; a three-stop gradient has nowhere to go.
    for (int i = 0; i < count_; i++) {
      hsv[i] = i == 0 ? start : end;
    }
  } else {
    const int half = count_ / 2;
    fill_gradient(hsv, 0, start, half, mid);
    fill_gradient(hsv, half, mid, count_ - 1, end);
  }

  for (int i = 0; i < count_; i++) {
    to_[i] = hsv[i];
  }
}

void Segment::buildTarget() {
  if (!lit_) {
    for (int i = 0; i < count_; i++) {
      to_[i] = CRGB::Black;
    }
    return;
  }

  switch (mode) {
  case SegmentMode::RANDOM_GRADIENT:
    fillGradient();
    break;
  case SegmentMode::CONSTANT:
  case SegmentMode::PULSE:
  case SegmentMode::BLINK: {
    for (int i = 0; i < count_; i++) {
      to_[i] = color;
    }
    break;
  }
  }
}

void Segment::advance(uint32_t now) {
  if (!isAttached()) {
    return;
  }
  snapshotFrom(now); // must run before cycleStart_ moves
  cycleStart_ = now;

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

void Segment::tick(uint32_t now) {
  if (!isAttached()) {
    return;
  }
  if (now - cycleStart_ >= cycleMs) {
    advance(now);
  }
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
  case SegmentMode::CONSTANT:
  case SegmentMode::RANDOM_GRADIENT: {
    const uint8_t amount = fadeAmount(elapsed);
    for (int i = 0; i < count_; i++) {
      CRGB c = blend(from_[i], to_[i], amount);
      c.nscale8_video(brightness);
      strip_[start_ + i] = c;
    }
    break;
  }

  case SegmentMode::PULSE: {
    // A full dim-up and dim-down across one cycle. quadwave8 starts and ends at
    // zero, so consecutive cycles join without a seam and no crossfade is
    // needed.
    uint8_t phase = 0;
    if (cycleMs > 0) {
      const uint32_t p = (elapsed * 255UL) / cycleMs;
      phase = p >= 255 ? 255 : static_cast<uint8_t>(p);
    }
    const uint8_t level = lit_ ? quadwave8(phase) : 0;
    CRGB c = color;
    c.nscale8_video(scale8(level, brightness));

    for (int i = 0; i < count_; i++) {
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
