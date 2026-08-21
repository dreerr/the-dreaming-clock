#include "modes.h"

#include <Arduino.h>
#include <FastLED.h>

#include "animation.h"
#include "clock_time.h"
#include "config.h"
#include "display.h"
#include "dreams.h"
#include "leds.h"
#include "schedule.h"
#include "segment.h"
#include "settings.h"

// ===========================================================================
// Timing
// ===========================================================================
namespace {

constexpr uint32_t WAKEUP_DURATION_MS = 15000;

// A word has to outlast the slowest segment cycle. Segments only re-roll their
// probability at a cycle boundary, so with cycles up to DREAM_CYCLE_MAX_MS the
// display lags the probability ramp by up to that much. Measured on hardware: a
// 15 s window only reached full contrast at t=13.6 s, just as it was ending.
// The plateau below is therefore longer than DREAM_CYCLE_MAX_MS, which gives
// every segment time to roll at least once while the word is at full strength.
constexpr uint32_t DREAM_WORD_DISPLAY_MS = 30000;
constexpr uint32_t DREAM_WORD_RAMP_PERCENT = 20; // -> 6 s in, 18 s hold, 6 s out
constexpr uint32_t DREAM_WORD_PAUSE_MS = 30000;

// How strongly the ambient noise flickers while no word is showing.
constexpr uint8_t DREAM_NOISE_PROBABILITY = 150;

// Above this word strength the display settles to steady gradients so the word
// can be read. A word is only legible because its segments hold still; motion
// resumes as it fades, which makes the stillness part of the effect rather than
// a compromise.
constexpr uint8_t DREAM_SETTLE_ABOVE = 120;

// Per-segment cycle length while dreaming. The spread is the point: each
// segment settles at its own pace, so a word assembles unevenly instead of
// cross-fading in as a block.
//
// The floor was 1 s, which is what made the display twitchy — a segment could
// re-roll its whole appearance every second. The moving modes get a higher
// floor still: a sweep should drift across the bar, not dart across it.
//
// Nothing may exceed DREAM_CYCLE_MAX_MS, because the word plateau (18 s, see
// above) has to outlast the slowest cycle for a word to fully form.
constexpr uint16_t DREAM_CYCLE_MIN_MS = 4000;
constexpr uint16_t DREAM_CYCLE_MAX_MS = 16000;
constexpr uint16_t MOTION_CYCLE_MIN_MS = 11000;

// How long a segment takes to hand over when it changes animation.
constexpr uint16_t MODE_CROSSFADE_MS = 3000;

// Settling for a word is staggered across the segments rather than done in one
// frame — 29 bars all changing at the same instant reads as a jump however
// smoothly each one is cross-faded.
constexpr uint32_t SETTLE_STAGGER_MS = 70;

constexpr uint16_t WAKEUP_FADE_MS = 1200;
constexpr uint16_t TIME_NOT_SET_BLINK_MS = 2000;

// ===========================================================================
// A one-shot timer. Replaces the external Timer library, which was unpinned,
// GPL-licensed, and handed out slot indices that went stale after firing.
// ===========================================================================
struct Deadline {
  uint32_t at = 0;
  bool armed = false;

  void arm(uint32_t now, uint32_t delayMs) {
    at = now + delayMs;
    armed = true;
  }
  void disarm() { armed = false; }

  // Rollover-safe: signed difference, so this still works across the ~49-day
  // millis() wrap.
  bool due(uint32_t now) {
    if (!armed || static_cast<int32_t>(now - at) < 0) {
      return false;
    }
    armed = false;
    return true;
  }
};

Deadline sleepAgain;
Deadline autoWakeup;
Deadline dreamWordChange;

// ===========================================================================
// State
// ===========================================================================
DisplayMode mode = DisplayMode::DREAM;
volatile bool wakeupRequested = false;

CHSV mainColor = CHSV(0, 255, 255);

bool showingWord = false;
const char *currentWord = nullptr;
uint32_t wordStartedAt = 0;

const char *const kModeNames[] = {"off", "time-not-set", "dream", "pattern",
                                  "wakeup"};

uint8_t dreamBrightness() { return clockSettings.dreamBrightness; }
uint8_t wakeBrightness() { return clockSettings.brightness; }

// Which animation each segment is currently running, and the cycle counter it
// was chosen on. Re-rolling per cycle rather than per frame matters:
// renderDream() runs 60x a second and must not keep re-deciding underneath a
// segment mid-animation.
SegmentMode dreamModes[NUM_SEGMENTS];
uint16_t dreamModeCycle[NUM_SEGMENTS];
uint32_t settleStartedAt = 0;

// Weighted towards the steady gradient so the established character stays
// dominant and the motion is a seasoning rather than a light show. Only
// gradient modes appear here — PULSE and BLINK paint the flat `color`, which
// dream mode never sets.
SegmentMode rollDreamMode() {
  const uint8_t r = random8(100);
  if (r < 60) {
    return SegmentMode::RANDOM_GRADIENT;
  }
  return r < 80 ? SegmentMode::SWEEP : SegmentMode::BLOOM;
}

// Each segment gets its own palette character, rather than every bar drawing
// from one shared pool. The global spread decides how far a segment's centre
// may wander from the theme; its own spread and stop count decide whether it
// reads as one colour, a few related ones, or a rainbow.
void rollSegmentPalette(Segment &segment, uint8_t themeHue, uint8_t themeSpread) {
  segment.hueBase = themeHue + random8(themeSpread);

  // The hues are picked at random *within* the spread, so the arc they actually
  // cover is roughly spread x (stops-1)/(stops+1) — a two-stop bar from a wide
  // range often still lands on two near-identical colours. The tiers below are
  // set from what the strip measured, not from the nominal spread.
  const uint8_t character = random8(100);
  if (character < 30) {
    segment.hueSpread = random8(8, 30); // near enough one colour
    segment.gradientStops = random8(2, 4);
  } else if (character < 75) {
    segment.hueSpread = random8(60, 140); // a few clearly different colours
    segment.gradientStops = random8(3, 6);
  } else {
    segment.hueSpread = random8(160, 255); // rainbow
    segment.gradientStops = random8(4, 6);
  }

  // Signed, and never quite zero, so every gradient is going somewhere.
  int16_t drift = random8(35, 130);
  if (random8(2)) {
    drift = -drift;
  }
  segment.driftPercent = static_cast<int8_t>(drift);
}

// A moving segment gets a longer, narrower cycle range so its animation reads
// as a drift; a still one keeps the wider range so the display stays uneven.
void applyDreamMode(Segment &segment, SegmentMode mode) {
  segment.mode = mode;
  if (mode == SegmentMode::SWEEP || mode == SegmentMode::BLOOM) {
    segment.cycleMs = random(MOTION_CYCLE_MIN_MS, DREAM_CYCLE_MAX_MS);
    segment.fadeMs = MODE_CROSSFADE_MS;
  } else {
    segment.cycleMs = random(DREAM_CYCLE_MIN_MS, DREAM_CYCLE_MAX_MS);
    segment.fadeMs = segment.cycleMs / 2;
  }
}

void randomiseDreamCycles() {
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].cycleMs = random(DREAM_CYCLE_MIN_MS, DREAM_CYCLE_MAX_MS);
    segments[i].fadeMs = segments[i].cycleMs / 2;
  }
}

// -------------------------------------------------------------------------
// Dream words
// -------------------------------------------------------------------------

void startDreamWord(uint32_t now) {
  if (mode != DisplayMode::DREAM) {
    return;
  }
  currentWord = nextDreamWord();
  showingWord = true;
  wordStartedAt = now;
  // Deliberately does NOT re-randomise the cycles. Rewriting fadeMs for every
  // segment mid-cycle moves each one's cross-fade position instantly, which
  // snapped 15 bars to their targets at once the moment a word began — a
  // 182/255 jump, measured. Segments already get their own cycle length when
  // they roll a new mode.
  dreamWordChange.arm(now, DREAM_WORD_DISPLAY_MS);
  Serial.printf("[DREAM] Word: %s\n", currentWord);
}

void endDreamWord(uint32_t now) {
  showingWord = false;
  currentWord = nullptr;
  if (mode == DisplayMode::DREAM) {
    dreamWordChange.arm(now, DREAM_WORD_PAUSE_MS);
  }
}

// Ramp: fade in over the first 30%, hold, fade out over the last 30%.
uint8_t wordProbabilityAt(uint32_t elapsed) {
  const uint32_t ramp = DREAM_WORD_DISPLAY_MS * DREAM_WORD_RAMP_PERCENT / 100;
  if (elapsed >= DREAM_WORD_DISPLAY_MS) {
    return 0;
  }
  if (elapsed < ramp) {
    return static_cast<uint8_t>((elapsed * 255UL) / ramp);
  }
  const uint32_t fadeOutStart = DREAM_WORD_DISPLAY_MS - ramp;
  if (elapsed > fadeOutStart) {
    const uint32_t into = elapsed - fadeOutStart;
    return static_cast<uint8_t>(255UL - (into * 255UL) / ramp);
  }
  return 255;
}

void renderDream(uint32_t now, bool withWords) {
  // The theme: a drifting hue, and a spread that breathes between
  // near-monochrome and the whole wheel over a few minutes. Each segment picks
  // its own centre within that spread when its cycle rolls, so the display is
  // no longer one palette applied 29 times.
  const uint8_t themeHue = dreamHueBase(now);
  const uint8_t themeSpread = dreamHueSpread(now);

  const uint8_t wordProbability =
      (withWords && showingWord && currentWord != nullptr)
          ? wordProbabilityAt(now - wordStartedAt)
          : 0;
  const bool settle = wordProbability > DREAM_SETTLE_ABOVE;
  if (settle && settleStartedAt == 0) {
    settleStartedAt = now;
  } else if (!settle) {
    settleStartedAt = 0;
  }

  for (int i = 0; i < NUM_SEGMENTS; i++) {
    Segment &segment = segments[i];
    segment.brightness = dreamBrightness();

    // Pick a fresh animation and palette whenever this segment starts a new
    // cycle. The colon is two LEDs, where both moving envelopes degenerate.
    if (segment.cycles() != dreamModeCycle[i]) {
      dreamModeCycle[i] = segment.cycles();
      dreamModes[i] = (i == COLON_INDEX) ? SegmentMode::RANDOM_GRADIENT
                                         : rollDreamMode();
      applyDreamMode(segment, settle ? SegmentMode::RANDOM_GRADIENT
                                     : dreamModes[i]);
      rollSegmentPalette(segment, themeHue, themeSpread);
      segment.refreshTarget(); // the target was built before the palette moved
    } else if (settle && segment.mode != SegmentMode::RANDOM_GRADIENT &&
               (now - settleStartedAt) >=
                   static_cast<uint32_t>(i) * SETTLE_STAGGER_MS) {
      // Hand over rather than cut, and one bar at a time.
      segment.transitionTo(SegmentMode::RANDOM_GRADIENT, now,
                           MODE_CROSSFADE_MS);
    }
  }

  if (wordProbability == 0) {
    setAllDigits(DREAM_NOISE_PROBABILITY);
    segments[COLON_INDEX].probability = DREAM_NOISE_PROBABILITY / 3;
    return;
  }

  // As the word firms up, the surrounding noise recedes — so the word
  // condenses out of the noise rather than being pasted over it.
  const uint8_t noise = scale8(DREAM_NOISE_PROBABILITY, 255 - wordProbability);

  setWordOverNoise(currentWord, wordProbability, noise);
  segments[COLON_INDEX].probability = noise / 3;
}

void renderTimeNotSet() {
  setAllMode(SegmentMode::BLINK, TIME_NOT_SET_BLINK_MS, 0);
  setAllColor(CRGB(mainColor), wakeBrightness());
  setNumber(0, 255);
  segments[COLON_INDEX].probability = 255;
}

void renderWakeup() {
  const DateTime local = nowLocal();
  setNumber(local.minute() + local.hour() * 100, 255);

  for (int i = 0; i < NUM_DIGIT_SEGMENTS; i++) {
    segments[i].mode = SegmentMode::CONSTANT;
    segments[i].brightness = wakeBrightness();
    segments[i].color = CRGB(mainColor);
  }

  // BLINK is on for the first half of its cycle and off for the second, so a
  // 2 s cycle is exactly the one-second colon blink — no per-frame toggling.
  Segment &colon = segments[COLON_INDEX];
  colon.mode = SegmentMode::BLINK;
  colon.color = CRGB(mainColor);
  colon.brightness = wakeBrightness();
  colon.cycleMs = 2000;
  colon.probability = 255;
}

} // namespace

// ===========================================================================
// Public
// ===========================================================================

const char *displayModeName(DisplayMode m) {
  return kModeNames[static_cast<uint8_t>(m)];
}

DisplayMode currentDisplayMode() { return mode; }

void requestWakeup() { wakeupRequested = true; }

void enterDreamMode(uint32_t now) {
  Serial.println(F("[MODE] dream"));
  mode = DisplayMode::DREAM;
  showingWord = false;
  currentWord = nullptr;
  sleepAgain.disarm();
  setAllMode(SegmentMode::RANDOM_GRADIENT, DREAM_CYCLE_MAX_MS,
             DREAM_CYCLE_MAX_MS / 2);
  randomiseDreamCycles(); // per-segment cycle lengths, after setAllMode()
  dreamWordChange.arm(now, DREAM_WORD_PAUSE_MS);
  restartAll(now);
}

void enterPatternMode(uint32_t now) {
  Serial.println(F("[MODE] pattern"));
  mode = DisplayMode::PATTERN;
  showingWord = false;
  currentWord = nullptr;
  sleepAgain.disarm();
  dreamWordChange.disarm();
  randomiseDreamCycles();
  restartAll(now);
}

static void enterWakeupMode(uint32_t now) {
  Serial.println(F("[MODE] wakeup"));
  mode = DisplayMode::WAKEUP;
  showingWord = false;
  currentWord = nullptr;
  dreamWordChange.disarm();

  mainColor = CHSV(random8(), 255, 255);
  setAllMode(SegmentMode::CONSTANT, WAKEUP_FADE_MS, WAKEUP_FADE_MS);
  setAllColor(CRGB(mainColor), wakeBrightness());
  renderWakeup();
  restartAll(now);

  sleepAgain.arm(now, WAKEUP_DURATION_MS);
}

void scheduleAutoWakeup() {
  autoWakeup.disarm();

  if (!hasValidTime()) {
    return;
  }
  const DateTime local = nowLocal();
  const int gapMinutes = minutesToNextWakeupSlot(
      local.hour() * 60 + local.minute(), clockSettings.wakeupInterval);
  if (gapMinutes < 0) {
    return;
  }

  const uint32_t ms = static_cast<uint32_t>(gapMinutes) * 60000UL -
                      static_cast<uint32_t>(local.second()) * 1000UL;
  autoWakeup.arm(millis(), ms);
  Serial.printf("[WAKEUP] Next automatic wakeup in %lu s\n",
                static_cast<unsigned long>(ms / 1000));
}

void setupModes(uint32_t now) {
  mainColor = CHSV(random8(), 255, 255);
  enterDreamMode(now);
  scheduleAutoWakeup();
}

void updateMode(uint32_t now) {
  // --- timers, serviced in every mode -------------------------------------
  if (autoWakeup.due(now)) {
    wakeupRequested = true;
    scheduleAutoWakeup();
  }
  if (sleepAgain.due(now) && mode == DisplayMode::WAKEUP) {
    enterDreamMode(now);
  }
  if (dreamWordChange.due(now)) {
    if (showingWord) {
      endDreamWord(now);
    } else {
      startDreamWord(now);
    }
  }

  // --- explicit wakeup request --------------------------------------------
  if (wakeupRequested) {
    wakeupRequested = false;
    if (hasValidTime()) {
      const DateTime local = nowLocal();
      if (isDisplayActiveNow(local.dayOfTheWeek(), local.hour())) {
        enterWakeupMode(now);
        return;
      }
    }
  }

  // --- mode selection ------------------------------------------------------
  if (!hasValidTime()) {
    if (mode != DisplayMode::TIME_NOT_SET) {
      Serial.println(F("[MODE] time not set"));
      mode = DisplayMode::TIME_NOT_SET;
      restartAll(now);
    }
  } else if (mode == DisplayMode::TIME_NOT_SET) {
    // Time arrived (NTP or the web UI) — go back to dreaming.
    enterDreamMode(now);
    scheduleAutoWakeup();
  } else if (mode != DisplayMode::WAKEUP) {
    const DateTime local = nowLocal();
    const bool active = isDisplayActiveNow(local.dayOfTheWeek(), local.hour());
    if (!active) {
      if (mode != DisplayMode::OFF) {
        Serial.println(F("[MODE] off (outside active hours)"));
        mode = DisplayMode::OFF;
      }
      return;
    }
    if (mode == DisplayMode::OFF) {
      enterDreamMode(now);
    }
  }

  // --- render --------------------------------------------------------------
  switch (mode) {
  case DisplayMode::TIME_NOT_SET:
    renderTimeNotSet();
    break;
  case DisplayMode::DREAM:
    renderDream(now, true);
    break;
  case DisplayMode::PATTERN:
    renderDream(now, false);
    break;
  case DisplayMode::WAKEUP:
    renderWakeup();
    break;
  case DisplayMode::OFF:
    break;
  }
}
