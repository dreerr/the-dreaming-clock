#include <stdio.h>
#include <unity.h>

#include "dreams.h"
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

  return UNITY_END();
}
