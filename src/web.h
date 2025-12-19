#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>

#include "definitions.h"
#include "settings.h"

// RTC time setting function from rtc.h
extern void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                       int year);
extern RTC_DS1307 rtc;
extern bool rtcInitialized;

AsyncWebServer server(80);

// Helper: Send JSON response
void sendJsonResponse(AsyncWebServerRequest *request, bool success,
                      const char *message = nullptr) {
  JsonDocument doc;
  doc["success"] = success;
  if (message) {
    doc["message"] = message;
  }
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void setupWeb() {
  Serial.println("Setup Web");
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  MDNS.addService("http", "tcp", 80);

  // GET / - Main page (always show settings page now)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html");
  });

  // GET /adjust - Settings page
  server.on("/adjust", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/adjust.html");
  });

  // GET /settings - Settings page (alias for /adjust)
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/adjust.html");
  });

  // ===== API ENDPOINTS =====

  // GET /api/time - Get current RTC time
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    if (rtcInitialized) {
      DateTime now = rtc.now();
      doc["success"] = true;
      doc["hours"] = now.hour();
      doc["minutes"] = now.minute();
      doc["seconds"] = now.second();
      doc["day"] = now.day();
      doc["month"] = now.month();
      doc["year"] = now.year();
      doc["weekday"] = now.dayOfTheWeek();
    } else {
      doc["success"] = false;
      doc["message"] = "RTC not initialized";
    }
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // POST /api/time - Set RTC time
  server.on("/api/time", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasArg("hours") && request->hasArg("minutes") &&
        request->hasArg("day") && request->hasArg("month") &&
        request->hasArg("year")) {
      setRTCTime(request->arg("hours").toInt(), request->arg("minutes").toInt(),
                 0, request->arg("day").toInt(), request->arg("month").toInt(),
                 request->arg("year").toInt());
      sendJsonResponse(request, true, "Time saved");
    } else {
      sendJsonResponse(request, false, "Missing parameters");
    }
  });

  // GET /api/active-hours - Get active hours settings
  server.on("/api/active-hours", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["success"] = true;
    doc["enabled"] = clockSettings.useActiveHours;

    JsonArray days = doc["days"].to<JsonArray>();
    const char *dayNames[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};

    for (int i = 0; i < 7; i++) {
      JsonObject day = days.add<JsonObject>();
      day["name"] = dayNames[i];
      day["enabled"] = clockSettings.days[i].enabled;
      day["start"] = clockSettings.days[i].startHour;
      day["end"] = clockSettings.days[i].endHour;
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // POST /api/active-hours - Set active hours settings
  server.on("/api/active-hours", HTTP_POST, [](AsyncWebServerRequest *request) {
    // Global enable/disable
    if (request->hasArg("enabled")) {
      clockSettings.useActiveHours =
          request->arg("enabled") == "true" || request->arg("enabled") == "1";
    }

    // Per-day settings
    for (int i = 0; i < 7; i++) {
      String prefix = "day" + String(i);

      if (request->hasArg(prefix + "_enabled")) {
        String val = request->arg(prefix + "_enabled");
        clockSettings.days[i].enabled = (val == "true" || val == "1");
      }
      if (request->hasArg(prefix + "_start")) {
        clockSettings.days[i].startHour =
            request->arg(prefix + "_start").toInt();
      }
      if (request->hasArg(prefix + "_end")) {
        clockSettings.days[i].endHour = request->arg(prefix + "_end").toInt();
      }
    }

    saveActiveHours();
    sendJsonResponse(request, true, "Active hours saved");
  });

  // GET /api/wakeup-interval - Get wakeup interval
  server.on("/api/wakeup-interval", HTTP_GET,
            [](AsyncWebServerRequest *request) {
              JsonDocument doc;
              doc["success"] = true;
              doc["interval"] = clockSettings.wakeupInterval;

              String response;
              serializeJson(doc, response);
              request->send(200, "application/json", response);
            });

  // POST /api/wakeup-interval - Set wakeup interval
  server.on("/api/wakeup-interval", HTTP_POST,
            [](AsyncWebServerRequest *request) {
              if (request->hasArg("interval")) {
                clockSettings.wakeupInterval = request->arg("interval").toInt();
                saveWakeupInterval();
                sendJsonResponse(request, true, "Wakeup interval saved");
              } else {
                sendJsonResponse(request, false, "Missing interval parameter");
              }
            });

  // POST /wakeup - Manual wakeup trigger
  server.on("/wakeup", HTTP_POST, [](AsyncWebServerRequest *request) {
    wakeup = true;
    sendJsonResponse(request, true, "Wakeup triggered");
  });

  // Serve static files
  server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=600");

  // Captive portal: redirect all unknown requests to main page
  server.onNotFound(
      [](AsyncWebServerRequest *request) { request->redirect("/"); });

  server.begin();
}
