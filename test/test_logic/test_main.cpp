#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

#include "animation.h"
#include "config.h"
#include "dreams.h"
#include "layout.h"
#include "patterns.h"
#include "schedule.h"

// ---------------------------------------------------------------------------
// patterns
// ---------------------------------------------------------------------------

void test_digits_map_to_glyphs(void) {
  TEST_ASSERT_EQUAL_HEX8(0x77, glyphFor('0'));
  TEST_ASSERT_EQUAL_HEX8(0x44, glyphFor('1'));
  TEST_ASSERT_EQUAL_HEX8(0x7F, glyphFor('8'));
}

void test_one_uses_exactly_two_segments(void) {
  uint8_t g = glyphFor('1');
  int lit = 0;
  for (int i = 0; i < 7; i++) {
    if (g & (1 << i))
      lit++;
  }
  TEST_ASSERT_EQUAL_INT(2, lit);
}

void test_eight_lights_all_seven_segments(void) {
  TEST_ASSERT_EQUAL_HEX8(0x7F, glyphFor('8'));
}

void test_case_is_not_distinguishable(void) {
  // Documented limitation of a 7-segment cell: one glyph per letter.
  TEST_ASSERT_EQUAL_HEX8(glyphFor('B'), glyphFor('b'));
  TEST_ASSERT_EQUAL_HEX8(glyphFor('E'), glyphFor('e'));
  TEST_ASSERT_EQUAL_HEX8(glyphFor('P'), glyphFor('p'));
}

void test_unrenderable_chars_are_blank(void) {
  TEST_ASSERT_EQUAL_HEX8(0, glyphFor(' '));
  TEST_ASSERT_EQUAL_HEX8(0, glyphFor('!'));
  TEST_ASSERT_EQUAL_HEX8(0, glyphFor('\0'));
  TEST_ASSERT_FALSE(isRenderable(' '));
  TEST_ASSERT_TRUE(isRenderable('A'));
  TEST_ASSERT_TRUE(isRenderable('7'));
}

void test_every_dream_word_letter_is_renderable(void) {
  for (char c = 'A'; c <= 'Z'; c++) {
    TEST_ASSERT_TRUE(isRenderable(c));
    TEST_ASSERT_NOT_EQUAL(0, glyphFor(c));
  }
}

// ---------------------------------------------------------------------------
// schedule: active hours
// ---------------------------------------------------------------------------

static DaySchedule allDays(bool enabled, uint8_t start, uint8_t end) {
  DaySchedule d;
  d.enabled = enabled;
  d.startHour = start;
  d.endHour = end;
  return d;
}

static void fill(DaySchedule days[7], DaySchedule value) {
  for (int i = 0; i < 7; i++)
    days[i] = value;
}

void test_active_hours_disabled_is_always_on(void) {
  DaySchedule days[7];
  fill(days, allDays(false, 8, 18));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, false, 3, 4));
}

void test_same_day_window(void) {
  DaySchedule days[7];
  fill(days, allDays(true, 8, 18));
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 1, 7));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 8));  // start inclusive
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 17));
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 1, 18)); // end exclusive
}

void test_overnight_window(void) {
  DaySchedule days[7];
  fill(days, allDays(true, 22, 6));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 22));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 23));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 0));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 5));
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 1, 6));
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 1, 12));
}

void test_disabled_day_is_off(void) {
  DaySchedule days[7];
  fill(days, allDays(true, 8, 18));
  days[0].enabled = false; // Sunday
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 0, 12));
  TEST_ASSERT_TRUE(isDisplayActiveTime(days, true, 1, 12));
}

void test_zero_length_window_is_off(void) {
  DaySchedule days[7];
  fill(days, allDays(true, 9, 9));
  TEST_ASSERT_FALSE(isDisplayActiveTime(days, true, 1, 9));
}

// ---------------------------------------------------------------------------
// schedule: auto-wakeup slots
// ---------------------------------------------------------------------------

void test_wakeup_disabled(void) {
  TEST_ASSERT_EQUAL_INT(-1, minutesToNextWakeupSlot(10 * 60, 0));
  TEST_ASSERT_EQUAL_INT(-1, minutesToNextWakeupSlot(10 * 60, -5));
}

void test_wakeup_quarter_hourly(void) {
  TEST_ASSERT_EQUAL_INT(13, minutesToNextWakeupSlot(10 * 60 + 17, 15)); // 10:17 -> 10:30
  TEST_ASSERT_EQUAL_INT(15, minutesToNextWakeupSlot(10 * 60, 15));      // 10:00 -> 10:15
  TEST_ASSERT_EQUAL_INT(1, minutesToNextWakeupSlot(10 * 60 + 44, 15));  // 10:44 -> 10:45
}

// This is the case the old `minute % interval` maths got wrong: `minute` never
// exceeds 59, so an interval of 120 could never align.
void test_wakeup_intervals_larger_than_an_hour_align_to_midnight(void) {
  TEST_ASSERT_EQUAL_INT(83, minutesToNextWakeupSlot(37, 120));           // 00:37 -> 02:00
  TEST_ASSERT_EQUAL_INT(120, minutesToNextWakeupSlot(2 * 60, 120));      // 02:00 -> 04:00
  TEST_ASSERT_EQUAL_INT(23, minutesToNextWakeupSlot(3 * 60 + 37, 120));  // 03:37 -> 04:00
  TEST_ASSERT_EQUAL_INT(60, minutesToNextWakeupSlot(17 * 60, 360));      // 17:00 -> 18:00
}

void test_wakeup_slot_never_zero_or_negative(void) {
  const int intervals[] = {5, 15, 30, 60, 120, 180, 240, 360};
  for (int i = 0; i < 8; i++) {
    for (int m = 0; m < 1440; m++) {
      int gap = minutesToNextWakeupSlot(m, intervals[i]);
      TEST_ASSERT_GREATER_THAN_INT(0, gap);
      TEST_ASSERT_LESS_OR_EQUAL_INT(intervals[i], gap);
    }
  }
}

void test_wakeup_wraps_over_midnight(void) {
  TEST_ASSERT_EQUAL_INT(1, minutesToNextWakeupSlot(1439, 15));  // 23:59 -> 00:00
  TEST_ASSERT_EQUAL_INT(60, minutesToNextWakeupSlot(23 * 60, 360)); // 23:00 -> 00:00
}

// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// dream words
// ---------------------------------------------------------------------------

void test_word_list_is_not_empty(void) {
  TEST_ASSERT_GREATER_THAN_INT(50, dreamWordCount());
}

// The list used to contain "bEArHASE": a missing comma made two adjacent string
// literals concatenate silently, losing one word and producing an entry too
// long for the display.
void test_every_word_is_exactly_four_chars(void) {
  for (int i = 0; i < dreamWordCount(); i++) {
    const char *w = dreamWordAt(i);
    TEST_ASSERT_NOT_NULL(w);
    int len = 0;
    while (w[len] != '\0')
      len++;
    if (len != DREAM_WORD_LENGTH) {
      char msg[64];
      snprintf(msg, sizeof(msg), "word %d (\"%s\") is %d chars", i, w, len);
      TEST_FAIL_MESSAGE(msg);
    }
  }
}

void test_every_word_is_renderable(void) {
  for (int i = 0; i < dreamWordCount(); i++) {
    const char *w = dreamWordAt(i);
    for (int c = 0; c < DREAM_WORD_LENGTH; c++) {
      if (!isRenderable(w[c])) {
        char msg[64];
        snprintf(msg, sizeof(msg), "word \"%s\" has unrenderable '%c'", w, w[c]);
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
}

void test_no_case_insensitive_duplicates(void) {
  // Case does not survive rendering, so two words differing only in case would
  // show up as the same thing twice.
  for (int i = 0; i < dreamWordCount(); i++) {
    for (int j = i + 1; j < dreamWordCount(); j++) {
      bool same = true;
      for (int c = 0; c < DREAM_WORD_LENGTH; c++) {
        if (glyphFor(dreamWordAt(i)[c]) != glyphFor(dreamWordAt(j)[c])) {
          same = false;
          break;
        }
      }
      if (same) {
        char msg[64];
        snprintf(msg, sizeof(msg), "\"%s\" and \"%s\" render identically",
                 dreamWordAt(i), dreamWordAt(j));
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
}

void test_next_word_avoids_immediate_repeat(void) {
  seedDreamWords(12345);
  const char *prev = nextDreamWord();
  for (int i = 0; i < 200; i++) {
    const char *w = nextDreamWord();
    TEST_ASSERT_TRUE(w != prev);
    prev = w;
  }
}

// ---------------------------------------------------------------------------
// segment -> LED mapping
//
// LEDS_PER_SEGMENT is a compile-time constant, so the firmware only ever sees
// one value. These sweep the parameterised mapping across many, which is what
// makes changing that constant safe: the invariants below are the only thing
// the rest of the code relies on.
// ---------------------------------------------------------------------------

static void checkMappingIsSound(int digitSegments, int ledsPerSegment,
                                int colonLeds) {
  const int total = numLedsFor(digitSegments, ledsPerSegment, colonLeds);
  const int segments = digitSegments + 1; // + colon

  char msg[128];
  snprintf(msg, sizeof(msg), "digits=%d leds=%d colon=%d", digitSegments,
           ledsPerSegment, colonLeds);

  // Every LED belongs to exactly one segment.
  int *owner = (int *)malloc(sizeof(int) * total);
  for (int i = 0; i < total; i++)
    owner[i] = -1;

  for (int seg = 0; seg < segments; seg++) {
    const int start =
        segmentLedStartFor(seg, digitSegments, ledsPerSegment, colonLeds);
    const int count =
        segmentLedCountFor(seg, digitSegments, ledsPerSegment, colonLeds);

    TEST_ASSERT_GREATER_OR_EQUAL_INT_MESSAGE(0, start, msg);
    TEST_ASSERT_GREATER_THAN_INT_MESSAGE(0, count, msg);
    TEST_ASSERT_LESS_OR_EQUAL_INT_MESSAGE(total, start + count, msg);

    for (int i = start; i < start + count; i++) {
      TEST_ASSERT_EQUAL_INT_MESSAGE(-1, owner[i], msg); // no overlap
      owner[i] = seg;
    }
  }

  // No gaps.
  for (int i = 0; i < total; i++) {
    TEST_ASSERT_NOT_EQUAL_MESSAGE(-1, owner[i], msg);
  }

  // The strip starts at the first segment and ends at the last digit segment.
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      0, segmentLedStartFor(0, digitSegments, ledsPerSegment, colonLeds), msg);
  TEST_ASSERT_EQUAL_INT_MESSAGE(
      total,
      segmentLedStartFor(digitSegments - 1, digitSegments, ledsPerSegment,
                         colonLeds) +
          ledsPerSegment,
      msg);

  // The colon sits between the two halves of the display.
  const int colonStart =
      segmentLedStartFor(digitSegments, digitSegments, ledsPerSegment,
                         colonLeds);
  const int lastBefore =
      segmentLedStartFor(digitSegments / 2 - 1, digitSegments, ledsPerSegment,
                         colonLeds) +
      ledsPerSegment;
  const int firstAfter = segmentLedStartFor(digitSegments / 2, digitSegments,
                                            ledsPerSegment, colonLeds);
  TEST_ASSERT_EQUAL_INT_MESSAGE(lastBefore, colonStart, msg);
  TEST_ASSERT_EQUAL_INT_MESSAGE(colonStart + colonLeds, firstAfter, msg);

  free(owner);
}

void test_mapping_holds_for_every_led_count(void) {
  for (int leds = 1; leds <= 32; leds++) {
    for (int colon = 1; colon <= 4; colon++) {
      checkMappingIsSound(28, leds, colon);
    }
  }
}

// Not the shipped shape, but the mapping should not secretly depend on 4x7.
void test_mapping_holds_for_other_digit_counts(void) {
  const int digitCounts[] = {2, 4, 6, 8};
  for (int i = 0; i < 4; i++) {
    checkMappingIsSound(digitCounts[i] * 7, 10, 2);
  }
}

// The values config.h actually compiles with.
void test_configured_layout_matches_the_hardware(void) {
  TEST_ASSERT_EQUAL_INT(282, NUM_LEDS);
  TEST_ASSERT_EQUAL_INT(29, NUM_SEGMENTS);
  TEST_ASSERT_EQUAL_INT(140, COLON_LED_START);
  TEST_ASSERT_EQUAL_INT(0, segmentLedStart(0));
  TEST_ASSERT_EQUAL_INT(130, segmentLedStart(13));
  TEST_ASSERT_EQUAL_INT(142, segmentLedStart(14));
  TEST_ASSERT_EQUAL_INT(140, segmentLedStart(COLON_INDEX));
  TEST_ASSERT_EQUAL_INT(COLON_LEDS, segmentLedCount(COLON_INDEX));
  TEST_ASSERT_EQUAL_INT(LEDS_PER_SEGMENT, segmentLedCount(0));
}

// MAX_SEG_LEDS sizes Segment's buffers, so it has to cover the colon too.
void test_segment_buffer_covers_the_largest_segment(void) {
  for (int seg = 0; seg < NUM_SEGMENTS; seg++) {
    TEST_ASSERT_LESS_OR_EQUAL_INT(MAX_SEG_LEDS, segmentLedCount(seg));
  }
}

// ---------------------------------------------------------------------------
// animation maths
// ---------------------------------------------------------------------------

void test_waves_start_and_end_at_zero(void) {
  TEST_ASSERT_EQUAL_UINT8(0, triWave(0));
  TEST_ASSERT_EQUAL_UINT8(0, triWave(255));
  TEST_ASSERT_EQUAL_UINT8(0, quadWave(0));
  TEST_ASSERT_EQUAL_UINT8(0, quadWave(255));
}

void test_waves_peak_in_the_middle(void) {
  TEST_ASSERT_EQUAL_UINT8(255, triWave(127));
  TEST_ASSERT_EQUAL_UINT8(255, triWave(128));
  TEST_ASSERT_EQUAL_UINT8(255, quadWave(127));
  TEST_ASSERT_EQUAL_UINT8(255, quadWave(128));
}

void test_waves_rise_then_fall_without_jumps(void) {
  for (int p = 1; p <= 127; p++) {
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(triWave(p - 1), triWave(p));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(quadWave(p - 1), quadWave(p));
  }
  for (int p = 129; p <= 255; p++) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(triWave(p - 1), triWave(p));
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(quadWave(p - 1), quadWave(p));
  }
}

// The complaint this whole change answers: every segment always looked the same
// colour, because the spread was pinned at 48 forever.
void test_hue_spread_stays_in_range(void) {
  for (uint32_t t = 0; t < DREAM_COHERENCE_PERIOD_MS * 3; t += 250) {
    const uint8_t s = dreamHueSpread(t);
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(DREAM_SPREAD_MIN, s);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(DREAM_SPREAD_MAX, s);
  }
}

void test_hue_spread_reaches_both_extremes_within_one_period(void) {
  uint8_t lo = 255, hi = 0;
  for (uint32_t t = 0; t < DREAM_COHERENCE_PERIOD_MS; t += 100) {
    const uint8_t s = dreamHueSpread(t);
    if (s < lo) lo = s;
    if (s > hi) hi = s;
  }
  // Near-monochrome at one end, the whole wheel at the other.
  TEST_ASSERT_LESS_OR_EQUAL_UINT8(DREAM_SPREAD_MIN + 2, lo);
  TEST_ASSERT_GREATER_OR_EQUAL_UINT8(DREAM_SPREAD_MAX - 2, hi);
}

// A visible snap in how alike the colours are would be worse than no variation.
void test_hue_spread_changes_smoothly(void) {
  uint8_t prev = dreamHueSpread(0);
  for (uint32_t t = 250; t < DREAM_COHERENCE_PERIOD_MS * 2; t += 250) {
    const uint8_t s = dreamHueSpread(t);
    const int step = s > prev ? s - prev : prev - s;
    if (step > 4) {
      char msg[80];
      snprintf(msg, sizeof(msg), "spread jumped %d at t=%lu", step,
               (unsigned long)t);
      TEST_FAIL_MESSAGE(msg);
    }
    prev = s;
  }
}

void test_hue_spread_period_is_four_minutes(void) {
  // Find two successive troughs and check the gap.
  int first = -1, second = -1;
  for (uint32_t t = 1; t < DREAM_COHERENCE_PERIOD_MS * 3; t += 100) {
    if (dreamHueSpread(t) == DREAM_SPREAD_MIN &&
        dreamHueSpread(t - 100) != DREAM_SPREAD_MIN) {
      if (first < 0) first = (int)t;
      else if (second < 0) { second = (int)t; break; }
    }
  }
  TEST_ASSERT_GREATER_THAN_INT(0, first);
  TEST_ASSERT_GREATER_THAN_INT(0, second);
  const int period = second - first;
  TEST_ASSERT_INT_WITHIN(1000, (int)DREAM_COHERENCE_PERIOD_MS, period);
}

// Every LED must be visited, or part of the bar would simply never light.
void test_sweep_reaches_every_led(void) {
  const int count = 10;
  for (int i = 0; i < count; i++) {
    uint8_t best = 0;
    for (int p = 0; p <= 255; p++) {
      const uint8_t l = sweepLevel((uint8_t)p, i, count);
      if (l > best) best = l;
    }
    if (best < 250) {
      char msg[64];
      snprintf(msg, sizeof(msg), "LED %d peaks at only %u", i, best);
      TEST_FAIL_MESSAGE(msg);
    }
  }
}

void test_sweep_head_travels_out_and_back(void) {
  const int count = 10;
  // The head is a plateau several LEDs wide, so several share the peak value.
  // Its centre of brightness is the meaningful position.
  auto centroid = [&](int phase) {
    long weighted = 0, total = 0;
    for (int i = 0; i < count; i++) {
      const long l = sweepLevel((uint8_t)phase, i, count);
      weighted += i * l;
      total += l;
    }
    return total > 0 ? (double)weighted / total : 0.0;
  };
  const double start = centroid(0);
  const double middle = centroid(127);
  const double end = centroid(255);

  TEST_ASSERT_TRUE(start < 1.5);            // parked at the near end
  TEST_ASSERT_TRUE(middle > count - 2.5);   // reached the far end
  TEST_ASSERT_TRUE(end < 1.5);              // and came back

  // and got there without reversing on the way
  double prev = start;
  for (int p = 1; p <= 127; p++) {
    const double c = centroid(p);
    TEST_ASSERT_TRUE(c >= prev - 0.01);
    prev = c;
  }
}

void test_sweep_is_dark_away_from_the_head(void) {
  // With the head at one end, the far end must be off.
  TEST_ASSERT_EQUAL_UINT8(0, sweepLevel(0, 9, 10));
  TEST_ASSERT_EQUAL_UINT8(0, sweepLevel(127, 0, 10));
}

void test_bloom_is_symmetric_about_the_centre(void) {
  const int count = 10;
  for (int p = 0; p <= 255; p++) {
    for (int i = 0; i < count; i++) {
      const uint8_t a = bloomLevel((uint8_t)p, i, count);
      const uint8_t b = bloomLevel((uint8_t)p, count - 1 - i, count);
      if (a != b) {
        char msg[80];
        snprintf(msg, sizeof(msg), "phase %d: LED %d=%u but mirror=%u", p, i, a, b);
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
}

void test_bloom_grows_from_the_centre_to_the_ends(void) {
  const int count = 10;
  // Each half starts having no effect at all, which is what makes the loop
  // seamless: at phase 0 the bar is still dark from the end of the last cycle.
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, bloomLevel(0, i, count));
  }
  // Shortly after, the middle is lit and the ends are not.
  TEST_ASSERT_GREATER_THAN_UINT8(0, bloomLevel(40, count / 2, count));
  TEST_ASSERT_EQUAL_UINT8(0, bloomLevel(40, 0, count));
  // By the end of the first half the whole bar is lit.
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_UINT8(255, bloomLevel(127, i, count));
  }
}

// The second half wipes the bar clean with darkness spreading from the centre,
// rather than the light shrinking back the way it came.
void test_bloom_erases_from_the_centre_in_the_second_half(void) {
  const int count = 10;
  // The erase also starts with no effect, so the turn is invisible.
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_UINT8(255, bloomLevel(128, i, count));
  }
  // Then the centre goes dark first while the ends stay lit.
  TEST_ASSERT_LESS_THAN_UINT8(bloomLevel(180, 0, count),
                              bloomLevel(180, count / 2, count));
  TEST_ASSERT_EQUAL_UINT8(255, bloomLevel(180, 0, count));
  // By the end everything is dark.
  for (int i = 0; i < count; i++) {
    TEST_ASSERT_EQUAL_UINT8(0, bloomLevel(255, i, count));
  }
}

// Measured on hardware, the seam where bloom turned from growing to erasing
// punched a full-depth notch into the middle of the bar in a single frame —
// changes of up to 148/255. No envelope may step like that.
void test_envelopes_have_no_visible_seams(void) {
  const int count = 10;
  struct { const char *name; uint8_t (*fn)(uint8_t, int, int); } envelopes[] = {
      {"sweep", sweepLevel}, {"bloom", bloomLevel}};

  for (auto &e : envelopes) {
    for (int i = 0; i < count; i++) {
      for (int p = 0; p < 255; p++) {
        const int step = (int)e.fn((uint8_t)(p + 1), i, count) -
                         (int)e.fn((uint8_t)p, i, count);
        if (step > 40 || step < -40) {
          char msg[96];
          snprintf(msg, sizeof(msg), "%s LED %d steps %d between phase %d and %d",
                   e.name, i, step, p, p + 1);
          TEST_FAIL_MESSAGE(msg);
        }
      }
      // and across the wrap back to the start of the next cycle
      const int wrap =
          (int)e.fn(0, i, count) - (int)e.fn(255, i, count);
      if (wrap > 40 || wrap < -40) {
        char msg[96];
        snprintf(msg, sizeof(msg), "%s LED %d steps %d across the wrap", e.name,
                 i, wrap);
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
}

// A single point darting along the bar was too nervous to watch.
void test_sweep_lights_several_leds_at_once(void) {
  const int count = 10;
  int worst = count;
  for (int p = 0; p <= 255; p++) {
    int on = 0;
    for (int i = 0; i < count; i++) {
      if (sweepLevel((uint8_t)p, i, count) > 40) on++;
    }
    if (on < worst) worst = on;
  }
  if (worst < 4) {
    char msg[64];
    snprintf(msg, sizeof(msg), "only %d LEDs lit at the thinnest point", worst);
    TEST_FAIL_MESSAGE(msg);
  }
}

void test_ramp_up_is_monotonic_end_to_end(void) {
  TEST_ASSERT_EQUAL_UINT8(0, rampUp(0));
  TEST_ASSERT_EQUAL_UINT8(255, rampUp(255));
  for (int p = 1; p <= 255; p++) {
    TEST_ASSERT_GREATER_OR_EQUAL_UINT8(rampUp(p - 1), rampUp(p));
  }
}

void test_envelopes_ignore_out_of_range_leds(void) {
  TEST_ASSERT_EQUAL_UINT8(0, sweepLevel(60, -1, 10));
  TEST_ASSERT_EQUAL_UINT8(0, sweepLevel(60, 10, 10));
  TEST_ASSERT_EQUAL_UINT8(0, bloomLevel(60, -1, 10));
  TEST_ASSERT_EQUAL_UINT8(0, bloomLevel(60, 10, 10));
}

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_digits_map_to_glyphs);
  RUN_TEST(test_one_uses_exactly_two_segments);
  RUN_TEST(test_eight_lights_all_seven_segments);
  RUN_TEST(test_case_is_not_distinguishable);
  RUN_TEST(test_unrenderable_chars_are_blank);
  RUN_TEST(test_every_dream_word_letter_is_renderable);

  RUN_TEST(test_active_hours_disabled_is_always_on);
  RUN_TEST(test_same_day_window);
  RUN_TEST(test_overnight_window);
  RUN_TEST(test_disabled_day_is_off);
  RUN_TEST(test_zero_length_window_is_off);

  RUN_TEST(test_wakeup_disabled);
  RUN_TEST(test_wakeup_quarter_hourly);
  RUN_TEST(test_wakeup_intervals_larger_than_an_hour_align_to_midnight);
  RUN_TEST(test_wakeup_slot_never_zero_or_negative);
  RUN_TEST(test_wakeup_wraps_over_midnight);

  RUN_TEST(test_word_list_is_not_empty);
  RUN_TEST(test_every_word_is_exactly_four_chars);
  RUN_TEST(test_every_word_is_renderable);
  RUN_TEST(test_no_case_insensitive_duplicates);
  RUN_TEST(test_next_word_avoids_immediate_repeat);

  RUN_TEST(test_mapping_holds_for_every_led_count);
  RUN_TEST(test_mapping_holds_for_other_digit_counts);
  RUN_TEST(test_configured_layout_matches_the_hardware);
  RUN_TEST(test_segment_buffer_covers_the_largest_segment);

  RUN_TEST(test_waves_start_and_end_at_zero);
  RUN_TEST(test_waves_peak_in_the_middle);
  RUN_TEST(test_waves_rise_then_fall_without_jumps);
  RUN_TEST(test_hue_spread_stays_in_range);
  RUN_TEST(test_hue_spread_reaches_both_extremes_within_one_period);
  RUN_TEST(test_hue_spread_changes_smoothly);
  RUN_TEST(test_hue_spread_period_is_four_minutes);
  RUN_TEST(test_sweep_reaches_every_led);
  RUN_TEST(test_sweep_head_travels_out_and_back);
  RUN_TEST(test_sweep_is_dark_away_from_the_head);
  RUN_TEST(test_bloom_is_symmetric_about_the_centre);
  RUN_TEST(test_bloom_grows_from_the_centre_to_the_ends);
  RUN_TEST(test_bloom_erases_from_the_centre_in_the_second_half);
  RUN_TEST(test_envelopes_have_no_visible_seams);
  RUN_TEST(test_sweep_lights_several_leds_at_once);
  RUN_TEST(test_ramp_up_is_monotonic_end_to_end);
  RUN_TEST(test_envelopes_ignore_out_of_range_leds);

  return UNITY_END();
}
