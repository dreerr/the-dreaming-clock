#include "definitions.h"
#include <ArduinoOTA.h>
#include <ESP8266mDNS.h>

void setupOTA() {
  DEBUG.printf("Setup OTA\n");
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword((const char *)"kei6yahghohngooS");
  DEBUG.println("set pass");
  MDNS.addService("ota", "tcp", 8266);
  DEBUG.println("added MDNS");
  ArduinoOTA.onStart([]() { DEBUG.println("OTA Start\n"); });
  ArduinoOTA.onEnd([]() { DEBUG.println("OTA End\n"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      DEBUG.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      DEBUG.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      DEBUG.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      DEBUG.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      DEBUG.println("End Failed");
  });
  DEBUG.println("start begin");
  ArduinoOTA.begin();
  DEBUG.println("end begin");
}

void loopOTA() { ArduinoOTA.handle(); }
