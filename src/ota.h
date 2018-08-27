#include "definitions.h"
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

void setupOTA() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword((const char *)"kei6yahghohngooS");
  MDNS.addService("ota", "tcp", 8266);


  ArduinoOTA.onStart([]() {
    Serial.println("OTA Start\n");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA End\n");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void loopOTA(){
  ArduinoOTA.handle();
}
