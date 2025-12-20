#include <ArduinoOTA.h>
#include <ESPmDNS.h>

void setupOTA() {
  Serial.println("=== OTA Setup ===");

  ArduinoOTA.setHostname(HOSTNAME);
  Serial.printf("  Hostname: %s\n", HOSTNAME);

  ArduinoOTA.setPassword((const char *)"kei6yahghohngooS");
  Serial.println("  Password: ********");

  MDNS.addService("ota", "tcp", 3232);
  Serial.println("  mDNS service: ota (TCP:3232)");

  ArduinoOTA.onStart([]() { Serial.println("\n>>> OTA Update started..."); });
  ArduinoOTA.onEnd(
      []() { Serial.println("\n>>> OTA Update complete! Rebooting..."); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("  OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\n>>> OTA Error [%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Authentication failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connection failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End failed");
  });

  ArduinoOTA.begin();
  Serial.println("  OTA ready for updates");
  Serial.println("=================\n");
}

void loopOTA() { ArduinoOTA.handle(); }
