#include "settings.h"

#include <Preferences.h>

ClockSettings clockSettings;
NetworkSettings networkSettings;

namespace {

Preferences preferences;

// NVS keys are limited to 15 characters, so the per-day keys are kept terse.
void dayKeys(int day, char (&en)[8], char (&st)[8], char (&ed)[8]) {
  snprintf(en, sizeof(en), "day%den", day);
  snprintf(st, sizeof(st), "day%dst", day);
  snprintf(ed, sizeof(ed), "day%ded", day);
}

void loadNetwork() {
  networkSettings.mode =
      static_cast<NetworkMode>(preferences.getUChar("netMode", NETWORK_CAPTIVE));

  networkSettings.ssid[0] = '\0';
  networkSettings.password[0] = '\0';
  if (preferences.isKey("netSSID")) {
    preferences.getString("netSSID", networkSettings.ssid,
                          sizeof(networkSettings.ssid));
  }
  if (preferences.isKey("netPass")) {
    preferences.getString("netPass", networkSettings.password,
                          sizeof(networkSettings.password));
  }
  networkSettings.fallbackToCaptive = preferences.getBool("netFallback", true);
}

void loadClock() {
  clockSettings.useActiveHours = preferences.getBool("useActiveHrs", true);
  clockSettings.wakeupInterval = preferences.getUShort("wakeupInt", WAKEUP_OFF);
  clockSettings.brightness = preferences.getUChar("bright", 255);
  clockSettings.dreamBrightness = preferences.getUChar("dreamBright", 160);

  clockSettings.timezone[0] = '\0';
  if (preferences.isKey("timezone")) {
    preferences.getString("timezone", clockSettings.timezone,
                          sizeof(clockSettings.timezone));
  }
  if (clockSettings.timezone[0] == '\0') {
    strlcpy(clockSettings.timezone, "Europe/Vienna",
            sizeof(clockSettings.timezone));
  }

  // Defaults: Mon-Fri 08:00-18:00, weekend off.
  for (int i = 0; i < 7; i++) {
    char en[8], st[8], ed[8];
    dayKeys(i, en, st, ed);
    const bool defaultEnabled = (i >= 1 && i <= 5);
    clockSettings.days[i].enabled = preferences.getBool(en, defaultEnabled);
    clockSettings.days[i].startHour = preferences.getUChar(st, 8);
    clockSettings.days[i].endHour = preferences.getUChar(ed, 18);
  }
}

} // namespace

void setupSettings() {
  Serial.println(F("=== Settings ==="));
  preferences.begin(SETTINGS_NAMESPACE, false);

  loadNetwork();
  loadClock();

  Serial.printf("  Network: %s\n", networkSettings.mode == NETWORK_CAPTIVE
                                       ? "Captive Portal"
                                       : "WiFi Client");
  if (networkSettings.mode == NETWORK_CLIENT) {
    Serial.printf("  SSID: %s (password %s)\n", networkSettings.ssid,
                  strlen(networkSettings.password) > 0 ? "set" : "empty");
  }
  Serial.printf("  Timezone: %s\n", clockSettings.timezone);
  Serial.printf("  Wakeup interval: %u min\n", clockSettings.wakeupInterval);
  Serial.printf("  Brightness: %u (dream %u)\n", clockSettings.brightness,
                clockSettings.dreamBrightness);
  Serial.println(F("================\n"));
}

void saveActiveHours() {
  preferences.putBool("useActiveHrs", clockSettings.useActiveHours);
  for (int i = 0; i < 7; i++) {
    char en[8], st[8], ed[8];
    dayKeys(i, en, st, ed);
    preferences.putBool(en, clockSettings.days[i].enabled);
    preferences.putUChar(st, clockSettings.days[i].startHour);
    preferences.putUChar(ed, clockSettings.days[i].endHour);
  }
  Serial.println(F("[SETTINGS] Active hours saved"));
}

void saveWakeupInterval() {
  preferences.putUShort("wakeupInt", clockSettings.wakeupInterval);
  Serial.printf("[SETTINGS] Wakeup interval saved: %u min\n",
                clockSettings.wakeupInterval);
}

void saveTimezone() {
  preferences.putString("timezone", clockSettings.timezone);
  Serial.printf("[SETTINGS] Timezone saved: %s\n", clockSettings.timezone);
}

void saveBrightness() {
  preferences.putUChar("bright", clockSettings.brightness);
  preferences.putUChar("dreamBright", clockSettings.dreamBrightness);
  Serial.printf("[SETTINGS] Brightness saved: %u / dream %u\n",
                clockSettings.brightness, clockSettings.dreamBrightness);
}

void saveNetworkSettings() {
  preferences.putUChar("netMode", networkSettings.mode);
  preferences.putString("netSSID", networkSettings.ssid);
  preferences.putString("netPass", networkSettings.password);
  preferences.putBool("netFallback", networkSettings.fallbackToCaptive);
  Serial.println(F("[SETTINGS] Network settings saved"));
}

bool isDisplayActiveNow(uint8_t weekday, uint8_t hour) {
  return isDisplayActiveTime(clockSettings.days, clockSettings.useActiveHours,
                             weekday, hour);
}
