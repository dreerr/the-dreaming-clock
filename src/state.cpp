#include "state.h"

#include <Arduino.h>
#include <WiFi.h>

#include "clock_time.h"
#include "config.h"
#include "modes.h"
#include "net_wifi.h"
#include "settings.h"
#include "timezones.h"

namespace {

const char *const kDayNames[7] = {"sun", "mon", "tue", "wed",
                                  "thu", "fri", "sat"};

CommandResult ok(const char *message = nullptr) { return {true, message}; }
CommandResult fail(const char *message) { return {false, message}; }

uint8_t clampByte(long v) {
  if (v < 0)
    return 0;
  if (v > 255)
    return 255;
  return static_cast<uint8_t>(v);
}

bool isValidWakeupInterval(long v) {
  switch (v) {
  case 0:
  case 5:
  case 15:
  case 30:
  case 60:
  case 120:
  case 180:
  case 240:
  case 360:
    return true;
  default:
    return false;
  }
}

// --- individual commands ---------------------------------------------------

CommandResult setBrightness(JsonVariantConst v, bool dream) {
  if (!v.is<long>()) {
    return fail("brightness must be a number 0-255");
  }
  const uint8_t value = clampByte(v.as<long>());
  if (dream) {
    clockSettings.dreamBrightness = value;
  } else {
    clockSettings.brightness = value;
  }
  saveBrightness();
  return ok();
}

CommandResult setTimezone(JsonVariantConst v) {
  const char *name = v.as<const char *>();
  if (name == nullptr || strlen(name) == 0) {
    return fail("timezone must be a non-empty string");
  }
  if (posixTzFor(name) == nullptr) {
    return fail("unknown timezone");
  }
  strlcpy(clockSettings.timezone, name, sizeof(clockSettings.timezone));
  saveTimezone();
  applyTimezone();
  scheduleAutoWakeup();
  return ok();
}

CommandResult setWakeupInterval(JsonVariantConst v) {
  if (!v.is<long>() || !isValidWakeupInterval(v.as<long>())) {
    return fail("wakeupInterval must be 0, 5, 15, 30, 60, 120, 180, 240 or 360");
  }
  clockSettings.wakeupInterval = static_cast<uint16_t>(v.as<long>());
  saveWakeupInterval();
  scheduleAutoWakeup();
  return ok();
}

CommandResult setUseActiveHours(JsonVariantConst v) {
  if (!v.is<bool>()) {
    return fail("useActiveHours must be a boolean");
  }
  clockSettings.useActiveHours = v.as<bool>();
  saveActiveHours();
  return ok();
}

CommandResult setDays(JsonVariantConst v) {
  JsonArrayConst days = v.as<JsonArrayConst>();
  if (days.isNull() || days.size() != 7) {
    return fail("days must be an array of 7 entries, Sunday first");
  }
  int i = 0;
  for (JsonObjectConst day : days) {
    const long start = day["start"] | -1;
    const long end = day["end"] | -1;
    if (start < 0 || start > 23 || end < 0 || end > 23) {
      return fail("day start and end must be hours 0-23");
    }
    clockSettings.days[i].enabled = day["enabled"] | false;
    clockSettings.days[i].startHour = static_cast<uint8_t>(start);
    clockSettings.days[i].endHour = static_cast<uint8_t>(end);
    i++;
  }
  saveActiveHours();
  return ok();
}

CommandResult setTime(JsonVariantConst v) {
  JsonObjectConst t = v.as<JsonObjectConst>();
  if (t.isNull()) {
    return fail("time must be an object");
  }
  const long year = t["year"] | -1;
  const long month = t["month"] | -1;
  const long day = t["day"] | -1;
  const long hour = t["hour"] | -1;
  const long minute = t["minute"] | -1;
  const long second = t["second"] | 0;

  if (year < 2020 || year > 2099 || month < 1 || month > 12 || day < 1 ||
      day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
      second < 0 || second > 59) {
    return fail("time out of range");
  }

  setLocalTime(year, month, day, hour, minute, second);
  scheduleAutoWakeup();
  requestWakeup();
  return ok("time set");
}

CommandResult setMode(JsonVariantConst v) {
  const char *name = v.as<const char *>();
  if (name == nullptr) {
    return fail("mode must be a string");
  }
  const uint32_t now = millis();
  if (strcmp(name, "dream") == 0) {
    enterDreamMode(now);
    return ok();
  }
  if (strcmp(name, "pattern") == 0) {
    enterPatternMode(now);
    return ok();
  }
  if (strcmp(name, "wakeup") == 0) {
    requestWakeup();
    return ok();
  }
  return fail("mode must be dream, pattern or wakeup");
}

CommandResult setNetwork(JsonVariantConst v) {
  JsonObjectConst n = v.as<JsonObjectConst>();
  if (n.isNull()) {
    return fail("network must be an object");
  }

  if (n["mode"].is<long>()) {
    const long m = n["mode"].as<long>();
    if (m != NETWORK_CAPTIVE && m != NETWORK_CLIENT) {
      return fail("network.mode must be 0 (captive) or 1 (client)");
    }
    networkSettings.mode = static_cast<NetworkMode>(m);
  }
  if (n["ssid"].is<const char *>()) {
    strlcpy(networkSettings.ssid, n["ssid"].as<const char *>(),
            sizeof(networkSettings.ssid));
  }
  // An absent password keeps the stored one; an empty string clears it.
  if (n["password"].is<const char *>()) {
    strlcpy(networkSettings.password, n["password"].as<const char *>(),
            sizeof(networkSettings.password));
  }
  if (n["fallback"].is<bool>()) {
    networkSettings.fallbackToCaptive = n["fallback"].as<bool>();
  }

  saveNetworkSettings();

  if (n["apply"] | false) {
    // Deferred: the teardown must not run inside the AsyncTCP task.
    requestNetworkRestart();
    return ok("network settings saved, reconnecting");
  }
  return ok("network settings saved, restart to apply");
}

} // namespace

void serializeState(JsonObject out) {
  JsonObject device = out["device"].to<JsonObject>();
  device["hostname"] = HOSTNAME;
  device["uptimeMs"] = millis();
  device["freeHeap"] = ESP.getFreeHeap();

  JsonObject time = out["time"].to<JsonObject>();
  const DateTime local = nowLocal();
  time["valid"] = hasValidTime();
  time["year"] = local.year();
  time["month"] = local.month();
  time["day"] = local.day();
  time["hour"] = local.hour();
  time["minute"] = local.minute();
  time["second"] = local.second();
  time["weekday"] = local.dayOfTheWeek();
  time["timezone"] = clockSettings.timezone;
  time["rtcPresent"] = rtcPresent();
  time["ntpSynced"] = ntpSynced();

  JsonObject display = out["display"].to<JsonObject>();
  display["mode"] = displayModeName(currentDisplayMode());
  display["brightness"] = clockSettings.brightness;
  display["dreamBrightness"] = clockSettings.dreamBrightness;

  JsonObject schedule = out["schedule"].to<JsonObject>();
  schedule["useActiveHours"] = clockSettings.useActiveHours;
  schedule["wakeupInterval"] = clockSettings.wakeupInterval;
  JsonArray days = schedule["days"].to<JsonArray>();
  for (int i = 0; i < 7; i++) {
    JsonObject day = days.add<JsonObject>();
    day["name"] = kDayNames[i];
    day["enabled"] = clockSettings.days[i].enabled;
    day["start"] = clockSettings.days[i].startHour;
    day["end"] = clockSettings.days[i].endHour;
  }

  JsonObject network = out["network"].to<JsonObject>();
  network["mode"] = networkSettings.mode;
  network["activeMode"] = activeNetworkMode();
  network["ssid"] = networkSettings.ssid;
  network["hasPassword"] = strlen(networkSettings.password) > 0;
  network["fallback"] = networkSettings.fallbackToCaptive;
  network["connected"] = networkConnected();
  network["ip"] = networkAddress();
}

void serializeLayout(JsonObject out) {
  out["numLeds"] = NUM_LEDS;
  out["numSegments"] = NUM_SEGMENTS;
  out["digitSegments"] = NUM_DIGIT_SEGMENTS;
  out["segmentsPerDigit"] = SEGMENTS_PER_DIGIT;
  out["colonIndex"] = COLON_INDEX;
  out["ledsPerSegment"] = LEDS_PER_SEGMENT;
  out["colonLeds"] = COLON_LEDS;

  // The explicit per-segment ranges are the point: consumers read the results
  // of segmentLedStart() instead of reimplementing it.
  JsonArray segments = out["segments"].to<JsonArray>();
  for (int i = 0; i < NUM_SEGMENTS; i++) {
    JsonObject seg = segments.add<JsonObject>();
    seg["start"] = segmentLedStart(i);
    seg["count"] = segmentLedCount(i);
    seg["reversed"] = segmentIsReversed(i);
  }
}

CommandResult applyCommand(const char *key, JsonVariantConst value) {
  if (key == nullptr) {
    return fail("missing key");
  }
  if (strcmp(key, "brightness") == 0) {
    return setBrightness(value, false);
  }
  if (strcmp(key, "dreamBrightness") == 0) {
    return setBrightness(value, true);
  }
  if (strcmp(key, "timezone") == 0) {
    return setTimezone(value);
  }
  if (strcmp(key, "wakeupInterval") == 0) {
    return setWakeupInterval(value);
  }
  if (strcmp(key, "useActiveHours") == 0) {
    return setUseActiveHours(value);
  }
  if (strcmp(key, "days") == 0) {
    return setDays(value);
  }
  if (strcmp(key, "time") == 0) {
    return setTime(value);
  }
  if (strcmp(key, "mode") == 0) {
    return setMode(value);
  }
  if (strcmp(key, "network") == 0) {
    return setNetwork(value);
  }
  return fail("unknown setting");
}

CommandResult applyCommands(JsonObjectConst commands) {
  if (commands.isNull()) {
    return fail("expected a JSON object");
  }
  bool applied = false;
  for (JsonPairConst kv : commands) {
    const CommandResult result = applyCommand(kv.key().c_str(), kv.value());
    if (!result.ok) {
      return result;
    }
    applied = true;
  }
  if (!applied) {
    return fail("no settings provided");
  }
  return ok("saved");
}
