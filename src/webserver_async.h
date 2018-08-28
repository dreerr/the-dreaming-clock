#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <Time.h>

#include "definitions.h"

AsyncWebServer server(80);

void indexGet(AsyncWebServerRequest *request) {
  if(time_was_set) {
    request->send(SPIFFS, "/index.html");
  } else {
    request->send(SPIFFS, "/adjust.html");
  }
}

void adjustGet(AsyncWebServerRequest *request) {
  request->send(SPIFFS, "/adjust.html");
}

void adjustPost(AsyncWebServerRequest *request) {
  if (request->hasArg("hours") && request->hasArg("minutes")) {
    setTime(request->arg("hours").toInt(), request->arg("minutes").toInt(), 00,
            14, 12, 2015);
    request->send(SPIFFS, "/adjusted.html");
    time_was_set = true;
  } else {
    request->send(SPIFFS, "/adjust.html");
  }
}

void wakeupPost(AsyncWebServerRequest *request) {
  if (request->hasArg("wakeup")) {
    wakeup = true;
    request->send(200, "text/plain", "{error: 0}");
  } else {
    request->send(200, "text/plain", "{error: 1}");
  }
}

void setupWebserver() {
  DEBUG.printf("Setup Webserver\n");
  SPIFFS.begin();
  server.on("/", HTTP_GET, indexGet);
  server.on("/adjust", HTTP_GET, adjustGet);
  server.on("/adjust", HTTP_POST, adjustPost);
  server.on("/wakeup", HTTP_POST, wakeupPost);
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect(HTTPHOST);
  });
  server.begin();
  MDNS.addService("http", "tcp", 80);
}
