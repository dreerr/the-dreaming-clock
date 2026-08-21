#include "schedule.h"

static constexpr int kMinutesPerDay = 24 * 60;

bool isDisplayActiveTime(const DaySchedule days[7], bool useActiveHours,
                         uint8_t weekday, uint8_t hour) {
  if (!useActiveHours) {
    return true;
  }
  if (weekday > 6) {
    return true; // out-of-range input: fail open rather than blanking the clock
  }

  const DaySchedule &day = days[weekday];
  if (!day.enabled) {
    return false;
  }
  if (day.startHour == day.endHour) {
    return false; // zero-length window
  }
  if (day.startHour < day.endHour) {
    return hour >= day.startHour && hour < day.endHour;
  }
  // Overnight window, e.g. 22-6.
  return hour >= day.startHour || hour < day.endHour;
}

int minutesToNextWakeupSlot(int minutesSinceMidnight, int intervalMinutes) {
  if (intervalMinutes <= 0) {
    return -1;
  }
  if (minutesSinceMidnight < 0 || minutesSinceMidnight >= kMinutesPerDay) {
    return -1;
  }
  if (intervalMinutes >= kMinutesPerDay) {
    return kMinutesPerDay - minutesSinceMidnight; // once a day, at midnight
  }

  int next = ((minutesSinceMidnight / intervalMinutes) + 1) * intervalMinutes;
  if (next > kMinutesPerDay) {
    next = kMinutesPerDay; // wrap to midnight, which is always a slot
  }
  return next - minutesSinceMidnight;
}
