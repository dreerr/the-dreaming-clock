#include "modes.h"

#include <Arduino.h>
#include <FastLED.h>

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

// Per-segment cycle length while dreaming. The spread is the point: each
// segment settles at its own pace, so a word assembles unevenly instead of
// cross-fading in as a block.
constexpr uint16_t DREAM_CYCLE_MIN_MS = 1000;
constexpr uint16_t DREAM_CYCLE_MAX_MS = 15000;

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

uint8_t driftingHue(uint32_t now) {
  // One full trip around the colour wheel every ~100 seconds.
  return static_cast<uint8_t>((now / 400) & 0xFF);
}

uint8_t dreamBrightness() { return clockSettings.dreamBrightness; }
uint8_t wakeBrightness() { return clockSettings.brightness; }

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
  randomiseDreamCycles();
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
  setAllHue(driftingHue(now), 48);
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    segments[i].mode = SegmentMode::RANDOM_GRADIENT;
    segments[i].brightness = dreamBrightness();
  }

  if (!withWords || !showingWord || currentWord == nullptr) {
    setAllDigits(DREAM_NOISE_PROBABILITY);
    segments[COLON_INDEX].probability = DREAM_NOISE_PROBABILITY / 3;
    return;
  }

  const uint8_t wordProbability = wordProbabilityAt(now - wordStartedAt);
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
