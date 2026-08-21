#pragma once
#include <stdint.h>

// Active-hours and auto-wakeup scheduling maths. Arduino-free so it can be
// unit-tested on the host (see test/test_logic).

struct DaySchedule {
  bool enabled;      // display active on this day at all
  uint8_t startHour; // 0-23
  uint8_t endHour;   // 0-23, exclusive
};

// weekday: 0 = Sunday .. 6 = Saturday, matching RTClib's dayOfTheWeek().
// Supports overnight windows (start > end, e.g. 22-6).
bool isDisplayActiveTime(const DaySchedule days[7], bool useActiveHours,
                         uint8_t weekday, uint8_t hour);

// Minutes until the next auto-wakeup slot. Slots are aligned to midnight, so an
// interval of 120 fires at 00:00, 02:00, 04:00 ... regardless of when it was
// configured. Returns -1 when the interval is disabled (<= 0).
//
// Aligning to midnight rather than to the current minute is what makes
// intervals larger than 60 behave correctly; `minute % interval` cannot work
// because `minute` never exceeds 59.
int minutesToNextWakeupSlot(int minutesSinceMidnight, int intervalMinutes);
