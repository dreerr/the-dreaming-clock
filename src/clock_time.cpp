#include "clock_time.h"

#include <WiFi.h>
#include <Wire.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "settings.h"
#include "timezones.h"

namespace {

RTC_DS1307 rtc;
bool haveRtc = false;
bool synced = false;
bool timeValid = false;

bool sntpStarted = false;
uint32_t lastSyncCheckMs = 0;
uint32_t lastResyncMs = 0;

// Anything before 2023-01-01 means the clock has never been set.
constexpr time_t kMinValidEpoch = 1672531200;
constexpr uint32_t kSyncCheckIntervalMs = 2000;
constexpr uint32_t kResyncIntervalMs = 6UL * 60UL * 60UL * 1000UL; // 6 h

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.google.com";

// nowLocal() is called for every frame and several times per mode update.
// Caching keeps that to a few localtime_r() calls per second while staying
// accurate enough for a blinking colon.
constexpr uint32_t kNowCacheMs = 200;
uint32_t nowCachedAtMs = 0;
DateTime nowCached((uint32_t)0);
bool nowCacheValid = false;

const char *activePosixTz() {
  const char *posix = posixTzFor(clockSettings.timezone);
  return posix != nullptr ? posix : "UTC0";
}

void writeRtcFromSystem() {
  if (!haveRtc) {
    return;
  }
  const time_t utc = time(nullptr);
  if (utc < kMinValidEpoch) {
    return;
  }
  struct tm g;
  gmtime_r(&utc, &g);
  rtc.adjust(DateTime(g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour,
                      g.tm_min, g.tm_sec));
}

void startSntp() {
  // configTzTime applies the TZ string and kicks off SNTP without blocking.
  configTzTime(activePosixTz(), ntpServer1, ntpServer2);
  sntpStarted = true;
  Serial.printf("[TIME] SNTP started (%s, %s)\n", ntpServer1, ntpServer2);
}

} // namespace

void applyTimezone() {
  const char *posix = activePosixTz();
  setenv("TZ", posix, 1);
  tzset();
  nowCacheValid = false;
  Serial.printf("[TIME] Timezone %s -> TZ=%s\n", clockSettings.timezone, posix);

  if (sntpStarted) {
    // Re-issue so SNTP and the C library agree on the new zone.
    configTzTime(posix, ntpServer1, ntpServer2);
  }
}

void setupClockTime() {
  Serial.println(F("=== Time ==="));
  applyTimezone();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  haveRtc = rtc.begin();

  if (!haveRtc) {
    Serial.println(F("  DS1307 not found — system clock only"));
  } else if (!rtc.isrunning()) {
    Serial.println(F("  DS1307 present but not running (never set)"));
  } else {
    // The RTC holds UTC; seed the system clock from it. DateTime::unixtime()
    // treats the stored fields as UTC, which is exactly the convention here.
    const DateTime utc = rtc.now();
    const time_t seeded = static_cast<time_t>(utc.unixtime());
    if (seeded >= kMinValidEpoch) {
      struct timeval tv = {seeded, 0};
      settimeofday(&tv, nullptr);
      timeValid = true;
      Serial.printf("  Seeded from DS1307: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                    utc.year(), utc.month(), utc.day(), utc.hour(),
                    utc.minute(), utc.second());
    }
  }

  if (!timeValid) {
    Serial.println(F("  Time not set — set it at /settings or connect WiFi"));
  }
  Serial.println(F("============\n"));
}

void loopClockTime() {
  const uint32_t now = millis();

  // SNTP runs whenever a network is available, regardless of whether the RTC is
  // present. Previously it was only attempted when the RTC was missing, so a
  // present-but-wrong RTC never got corrected.
  if (!sntpStarted && WiFi.status() == WL_CONNECTED) {
    startSntp();
  }

  if (now - lastSyncCheckMs < kSyncCheckIntervalMs) {
    return;
  }
  lastSyncCheckMs = now;

  if (sntpStarted && time(nullptr) >= kMinValidEpoch) {
    if (!synced) {
      synced = true;
      timeValid = true;
      nowCacheValid = false;
      lastResyncMs = now;
      const DateTime local = nowLocal();
      Serial.printf("[TIME] NTP synced: %04d-%02d-%02d %02d:%02d:%02d local\n",
                    local.year(), local.month(), local.day(), local.hour(),
                    local.minute(), local.second());
      writeRtcFromSystem();
    } else if (now - lastResyncMs >= kResyncIntervalMs) {
      lastResyncMs = now;
      writeRtcFromSystem();
      Serial.println(F("[TIME] Periodic RTC refresh from system clock"));
    }
  }
}

DateTime nowLocal() {
  const uint32_t ms = millis();
  if (nowCacheValid && (ms - nowCachedAtMs) < kNowCacheMs) {
    return nowCached;
  }

  const time_t utc = time(nullptr);
  struct tm lt;
  localtime_r(&utc, &lt);

  nowCached = DateTime(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour,
                       lt.tm_min, lt.tm_sec);
  nowCachedAtMs = ms;
  nowCacheValid = true;
  return nowCached;
}

void setLocalTime(int year, int month, int day, int hour, int minute,
                  int second) {
  struct tm lt = {};
  lt.tm_year = year - 1900;
  lt.tm_mon = month - 1;
  lt.tm_mday = day;
  lt.tm_hour = hour;
  lt.tm_min = minute;
  lt.tm_sec = second;
  lt.tm_isdst = -1; // let the TZ rules decide

  const time_t utc = mktime(&lt); // interprets lt as local time, returns UTC
  if (utc < 0) {
    Serial.println(F("[TIME] Rejected invalid date"));
    return;
  }

  struct timeval tv = {utc, 0};
  settimeofday(&tv, nullptr);
  timeValid = true;
  nowCacheValid = false;
  writeRtcFromSystem();

  Serial.printf("[TIME] Set to %04d-%02d-%02d %02d:%02d:%02d local\n", year,
                month, day, hour, minute, second);
  if (!haveRtc) {
    Serial.println(F("  (no RTC — time is lost on power cycle)"));
  }
}

bool hasValidTime() { return timeValid; }
bool rtcPresent() { return haveRtc; }
bool ntpSynced() { return synced; }
