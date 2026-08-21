#include "timezones.h"

#include <string.h>

namespace {

struct TzEntry {
  const char *iana;
  const char *posix;
};

const TzEntry kZones[] = {
    {"Europe/London", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"Europe/Berlin", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Paris", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Rome", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Madrid", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Vienna", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Zurich", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Stockholm", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"Europe/Athens", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"Europe/Moscow", "MSK-3"},
    {"America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"America/Sao_Paulo", "<-03>3"},
    {"Asia/Tokyo", "JST-9"},
    {"Asia/Shanghai", "CST-8"},
    {"Asia/Singapore", "<+08>-8"},
    {"Asia/Dubai", "<+04>-4"},
    {"Australia/Sydney", "AEST-10AEDT,M10.1.0,M4.1.0/3"},
    {"Pacific/Auckland", "NZST-12NZDT,M9.5.0,M4.1.0/3"},
    {"UTC", "UTC0"},
};

constexpr int kZoneCount = sizeof(kZones) / sizeof(kZones[0]);

} // namespace

const char *posixTzFor(const char *ianaName) {
  if (ianaName == nullptr) {
    return nullptr;
  }
  for (int i = 0; i < kZoneCount; i++) {
    if (strcmp(ianaName, kZones[i].iana) == 0) {
      return kZones[i].posix;
    }
  }
  return nullptr;
}

int timezoneCount() { return kZoneCount; }

const char *timezoneNameAt(int index) {
  if (index < 0 || index >= kZoneCount) {
    return nullptr;
  }
  return kZones[index].iana;
}
