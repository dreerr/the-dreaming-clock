#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <FS.h>
#include <LittleFS.h>

#include "settings.h"
#include "websocket.h"

// RTC time setting function from rtc.h
extern void setRTCTime(int hours, int minutes, int seconds, int day, int month,
                       int year);
extern RTC_DS1307 rtc;
extern bool rtcInitialized;
extern bool usingInternalTime;
DateTime getCurrentTime();

// Network restart function from network.h
extern void restartNetwork();
extern NetworkMode activeNetworkMode;

// Mode functions from modes.h
extern void scheduleAutoWakeup();

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
  Serial.println("=== Web Server Setup ===");

  if (!LittleFS.begin()) {
    Serial.println("  ERROR: Failed to mount LittleFS filesystem!");
    return;
  }
  Serial.println("  LittleFS mounted successfully");

  MDNS.addService("http", "tcp", 80);
  Serial.printf("  URL: http://%s.local\n", HOSTNAME);
  Serial.println("  Port: 80");

  // GET / - Main page (always show settings page now)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html");
  });

  // GET /settings - Settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/settings.html");
  });

  // ===== API ENDPOINTS =====

  // GET /api/time - Get current time (RTC or internal fallback)
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    DateTime now = getCurrentTime();
    doc["success"] = true;
    doc["hours"] = now.hour();
    doc["minutes"] = now.minute();
    doc["seconds"] = now.second();
    doc["day"] = now.day();
    doc["month"] = now.month();
    doc["year"] = now.year();
    doc["weekday"] = now.dayOfTheWeek();
    doc["usingInternalTime"] = usingInternalTime;
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
      scheduleAutoWakeup();
      wakeup = true;
      sendJsonResponse(request, true, "Time saved");
    } else {
      sendJsonResponse(request, false, "Missing parameters");
    }
  });

  // GET /api/timezone - Get current timezone
  server.on("/api/timezone", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["success"] = true;
    doc["timezone"] = clockSettings.timezone;
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // POST /api/timezone - Set timezone
  server.on("/api/timezone", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasArg("timezone")) {
      String tz = request->arg("timezone");
      if (tz.length() > 0 && tz.length() < sizeof(clockSettings.timezone)) {
        strncpy(clockSettings.timezone, tz.c_str(),
                sizeof(clockSettings.timezone) - 1);
        clockSettings.timezone[sizeof(clockSettings.timezone) - 1] = '\0';
        saveTimezone();
        sendJsonResponse(request, true, "Timezone saved");
      } else {
        sendJsonResponse(request, false, "Invalid timezone");
      }
    } else {
      sendJsonResponse(request, false, "Missing timezone parameter");
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
                scheduleAutoWakeup();
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

  // GET /api/network - Get network settings
  server.on("/api/network", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["success"] = true;
    doc["mode"] = networkSettings.mode;
    doc["ssid"] = networkSettings.ssid;
    // Don't send password for security
    doc["hasPassword"] = strlen(networkSettings.password) > 0;
    doc["fallback"] = networkSettings.fallbackToCaptive;
    doc["activeMode"] = activeNetworkMode;
    doc["connected"] =
        (activeNetworkMode == NETWORK_CLIENT && WiFi.status() == WL_CONNECTED);
    if (activeNetworkMode == NETWORK_CLIENT && WiFi.status() == WL_CONNECTED) {
      doc["ip"] = WiFi.localIP().toString();
    } else if (activeNetworkMode == NETWORK_CAPTIVE) {
      doc["ip"] = "192.168.4.1";
    }

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });

  // POST /api/network - Set network settings
  server.on("/api/network", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool changed = false;

    if (request->hasArg("mode")) {
      int mode = request->arg("mode").toInt();
      if (mode == NETWORK_CAPTIVE || mode == NETWORK_CLIENT) {
        networkSettings.mode = (NetworkMode)mode;
        changed = true;
      }
    }

    if (request->hasArg("ssid")) {
      String ssid = request->arg("ssid");
      strncpy(networkSettings.ssid, ssid.c_str(),
              sizeof(networkSettings.ssid) - 1);
      networkSettings.ssid[sizeof(networkSettings.ssid) - 1] = '\0';
      changed = true;
    }

    if (request->hasArg("password")) {
      String password = request->arg("password");
      // Only update password if provided (allow empty to clear)
      strncpy(networkSettings.password, password.c_str(),
              sizeof(networkSettings.password) - 1);
      networkSettings.password[sizeof(networkSettings.password) - 1] = '\0';
      changed = true;
    }

    if (request->hasArg("fallback")) {
      String val = request->arg("fallback");
      networkSettings.fallbackToCaptive = (val == "true" || val == "1");
      changed = true;
    }

    if (changed) {
      saveNetworkSettings();

      // Check if we should restart network now
      if (request->hasArg("apply") &&
          (request->arg("apply") == "true" || request->arg("apply") == "1")) {
        sendJsonResponse(request, true,
                         "Network settings saved. Restarting network...");
        // Delay restart to allow response to be sent
        delay(100);
        restartNetwork();
      } else {
        sendJsonResponse(request, true,
                         "Network settings saved. Restart to apply.");
      }
    } else {
      sendJsonResponse(request, false, "No valid parameters provided");
    }
  });

  // Serve static files
  server.serveStatic("/", LittleFS, "/").setCacheControl("max-age=600");

  // Captive portal: redirect all unknown requests to main page
  server.onNotFound(
      [](AsyncWebServerRequest *request) { request->redirect("/"); });

  // Setup WebSocket for LED preview
  setupWebSocket(server);

  server.begin();
  Serial.println("  Web server started");
  Serial.println("========================\n");
}

// Call this from the main loop to handle WebSocket updates
void loopWeb() { loopWebSocket(); }
