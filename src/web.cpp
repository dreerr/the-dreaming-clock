#include "web.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "config.h"
#include "messages.h"
#include "modes.h"
#include "state.h"
#include "timezones.h"
#include "ws_preview.h"

namespace {

AsyncWebServer server(80);

void sendJson(AsyncWebServerRequest *request, int code, JsonDocument &doc) {
  String body;
  serializeJson(doc, body);
  request->send(code, "application/json", body);
}

void sendResult(AsyncWebServerRequest *request, const CommandResult &result) {
  JsonDocument doc;
  doc["success"] = result.ok;
  if (result.message != nullptr) {
    doc["message"] = result.message;
  }
  sendJson(request, result.ok ? 200 : 400, doc);
}

// ---------------------------------------------------------------------------
// JSON request bodies.
//
// AsyncCallbackJsonWebHandler is not usable here: ESPAsyncWebServer does not
// declare ArduinoJson as a dependency, so its AsyncJson.cpp compiles itself out
// via __has_include and the handler never gets linked. Accumulating the body by
// hand is a few lines and does not depend on library-resolution order.
// ---------------------------------------------------------------------------

constexpr size_t kMaxBodyBytes = 2048;

void collectBody(AsyncWebServerRequest *request, uint8_t *data, size_t len,
                 size_t index, size_t total) {
  if (total == 0 || total > kMaxBodyBytes) {
    return; // leaves _tempObject null; the request handler reports the error
  }
  if (index == 0) {
    if (request->_tempObject != nullptr) {
      free(request->_tempObject);
    }
    request->_tempObject = malloc(total + 1);
    if (request->_tempObject == nullptr) {
      return;
    }
    static_cast<char *>(request->_tempObject)[total] = '\0';
  }
  if (request->_tempObject != nullptr && index + len <= total) {
    memcpy(static_cast<char *>(request->_tempObject) + index, data, len);
  }
}

void handleStatePost(AsyncWebServerRequest *request) {
  if (request->_tempObject == nullptr) {
    sendResult(request, {false, "missing, oversized or unreadable body"});
    return;
  }
  JsonDocument doc;
  const DeserializationError error =
      deserializeJson(doc, static_cast<const char *>(request->_tempObject));
  if (error) {
    sendResult(request, {false, "invalid JSON"});
    return;
  }
  sendResult(request, applyCommands(doc.as<JsonObjectConst>()));
}

void handleMessagePost(AsyncWebServerRequest *request) {
  if (request->_tempObject == nullptr) {
    sendResult(request, {false, "missing, oversized or unreadable body"});
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, static_cast<const char *>(request->_tempObject))) {
    sendResult(request, {false, "invalid JSON"});
    return;
  }

  JsonDocument out;
  JsonObject report = out.to<JsonObject>();
  const CommandResult result = enqueueMessages(doc.as<JsonVariantConst>(), report);
  report["success"] = result.ok;
  if (result.message != nullptr) {
    report["message"] = result.message;
  }
  sendJson(request, result.ok ? 200 : 400, out);
}

void registerRoutes() {
  // --- state ---------------------------------------------------------------
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    serializeState(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  server.on("/api/state", HTTP_POST, handleStatePost, nullptr, collectBody);

  // --- messages -------------------------------------------------------------
  server.on("/api/message", HTTP_POST, handleMessagePost, nullptr, collectBody);

  server.on("/api/message", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    messageClearAll();
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "queue cleared";
    sendJson(request, 200, doc);
  });

  // --- LED layout, so the preview does not duplicate the mapping ------------
  server.on("/api/layout", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    serializeLayout(doc.to<JsonObject>());
    sendJson(request, 200, doc);
  });

  // --- timezone list, so the UI does not duplicate it -----------------------
  server.on("/api/timezones", HTTP_GET, [](AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray zones = doc.to<JsonArray>();
    for (int i = 0; i < timezoneCount(); i++) {
      zones.add(timezoneNameAt(i));
    }
    sendJson(request, 200, doc);
  });

  // --- wakeup --------------------------------------------------------------
  server.on("/api/wakeup", HTTP_POST, [](AsyncWebServerRequest *request) {
    requestWakeup();
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = "wakeup triggered";
    sendJson(request, 200, doc);
  });

  // --- icons Safari asks for and this device does not have ------------------
  // Registered before serveStatic so they win the handler search. Without them
  // every one of these probes costs four failed LittleFS opens inside the
  // AsyncTCP task and then a 302 to "/", which hands Safari an HTML document
  // where it asked for a PNG. A plain 404 is both cheaper and truthful.
  for (const char *icon : {"/favicon.ico", "/apple-touch-icon.png",
                           "/apple-touch-icon-precomposed.png"}) {
    server.on(icon, HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(404);
    });
  }

  // --- static files --------------------------------------------------------
  // serveStatic (unlike request->send) transparently serves a pre-compressed
  // "<file>.gz" with Content-Encoding: gzip, which is what the frontend build
  // step produces.
  server.serveStatic("/", LittleFS, "/")
      .setDefaultFile("index.html")
      .setCacheControl("max-age=600");

  server.onNotFound([](AsyncWebServerRequest *request) {
    // API paths must fail loudly. Redirecting them to the UI made a typo look
    // like a successful request, which would badly confuse any integration.
    if (request->url().startsWith("/api/")) {
      JsonDocument doc;
      doc["success"] = false;
      doc["message"] = "unknown endpoint";
      sendJson(request, 404, doc);
      return;
    }
    // Everything else feeds the captive portal.
    request->redirect("/");
  });
}

} // namespace

void setupWeb() {
  Serial.println(F("=== Web ==="));

  if (!LittleFS.begin()) {
    Serial.println(F("  ERROR: LittleFS mount failed — run 'pio run -t uploadfs'"));
  } else {
    Serial.println(F("  LittleFS mounted"));
  }

  registerRoutes();
  setupWebSocket(server);
  server.begin();

  Serial.printf("  http://%s.local\n", HOSTNAME);
  Serial.println(F("===========\n"));
}

void loopWeb() { loopWebSocket(); }
