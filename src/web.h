#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>

#include "definitions.h"

// RTC time setting function from rtc.h
extern void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                       int year);

AsyncWebServer server(80);

void setupWeb() {
  Serial.println("Setup Web");
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  MDNS.addService("http", "tcp", 80);

  // GET /
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (timeWasSet) {
      request->send(LittleFS, "/index.html");
    } else {
      request->send(LittleFS, "/adjust.html");
    }
  });

  // GET /adjust
  server.on("/adjust", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/adjust.html");
  });

  // POST /adjust
  server.on("/adjust", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasArg("hours") && request->hasArg("minutes")) {
      setRTCTime(request->arg("hours").toInt(), request->arg("minutes").toInt(),
                 0, request->arg("day").toInt(), request->arg("month").toInt(),
                 request->arg("yr").toInt());
      request->send(LittleFS, "/adjusted.html");
    } else {
      request->send(LittleFS, "/adjust.html");
    }
  });

  // POST /wakeup
  server.on("/wakeup", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasArg("wakeup")) {
      wakeup = true;
      request->send(200, "text/plain", "{error: 0}");
    } else {
      request->send(200, "text/plain", "{error: 1}");
    }
  });

  server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=600");
  server.onNotFound(
      [](AsyncWebServerRequest *request) { request->redirect(HTTPHOST); });
  server.begin();
}
