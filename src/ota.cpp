#include "ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"

void setupOTA() {
  Serial.println(F("=== OTA ==="));
  ArduinoOTA.setHostname(HOSTNAME);

  // An unset CLOCK_OTA_PASSWORD defines the macro as an empty string, which
  // would leave OTA effectively unauthenticated. Fall back loudly rather than
  // silently shipping an open update port.
  const char *password = OTA_PASSWORD;
  if (password[0] == '\0') {
    password = "dreaming";
    Serial.println(F("  WARNING: CLOCK_OTA_PASSWORD is not set."));
    Serial.println(F("  Using an insecure default — export it before building."));
  }
  ArduinoOTA.setPassword(password);

  ArduinoOTA.onStart([]() { Serial.println(F("\n[OTA] Update started")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("\n[OTA] Complete, rebooting")); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total > 0) {
      Serial.printf("[OTA] %u%%\r", (progress / (total / 100)));
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    const char *reason = "unknown";
    switch (error) {
    case OTA_AUTH_ERROR:
      reason = "authentication failed";
      break;
    case OTA_BEGIN_ERROR:
      reason = "begin failed";
      break;
    case OTA_CONNECT_ERROR:
      reason = "connect failed";
      break;
    case OTA_RECEIVE_ERROR:
      reason = "receive failed";
      break;
    case OTA_END_ERROR:
      reason = "end failed";
      break;
    }
    Serial.printf("\n[OTA] Error %u: %s\n", error, reason);
  });

  ArduinoOTA.begin();
  Serial.println(F("  Ready on port 3232"));
  Serial.println(F("===========\n"));
}

void loopOTA() { ArduinoOTA.handle(); }
