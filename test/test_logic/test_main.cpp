#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

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

  return UNITY_END();
}
