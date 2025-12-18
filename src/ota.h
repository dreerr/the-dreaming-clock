#include "definitions.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

void setupOTA() {
  Serial.printf("Setup OTA\n");
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword((const char *)"kei6yahghohngooS");
  Serial.println("set pass");
  MDNS.addService("ota", "tcp", 3232);
  Serial.println("added MDNS");
  ArduinoOTA.onStart([]() { Serial.println("OTA Start\n"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA End\n"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  Serial.println("start begin");
  ArduinoOTA.begin();
  Serial.println("end begin");
}

void loopOTA() { ArduinoOTA.handle(); }
