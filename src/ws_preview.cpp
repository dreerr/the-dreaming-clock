#include "ws_preview.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "leds.h"

namespace {

AsyncWebSocket ledSocket("/ws/leds");

uint32_t lastSendMs = 0;
uint32_t droppedFrames = 0;
uint32_t lastDropReportMs = 0;

void handleTextCommand(uint8_t *data, size_t len) {
  char cmd[32];
  const size_t n = len < sizeof(cmd) - 1 ? len : sizeof(cmd) - 1;
  memcpy(cmd, data, n);
  cmd[n] = '\0';

  if (strcmp(cmd, "calibrate on") == 0) {
    setCalibration(true);
  } else if (strcmp(cmd, "calibrate off") == 0) {
    setCalibration(false);
  }
}

void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  switch (type) {
  case WS_EVT_CONNECT:
    Serial.printf("[WS] Client #%u connected\n", client->id());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
    if (calibrationActive() && ledSocket.count() == 0) {
      setCalibration(false); // don't leave the clock stuck in calibration
    }
    break;
  case WS_EVT_DATA: {
    AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
      handleTextCommand(data, len);
    }
    break;
  }
  default:
    break;
  }
}

} // namespace

void setupWebSocket(AsyncWebServer &server) {
  ledSocket.onEvent(onWsEvent);
  server.addHandler(&ledSocket);
  Serial.printf("[WS] /ws/leds ready (%d bytes/frame, %d FPS)\n", NUM_LEDS * 3,
                1000 / (int)PREVIEW_INTERVAL_MS);
}

void loopWebSocket() {
  const uint32_t now = millis();
  if (now - lastSendMs < PREVIEW_INTERVAL_MS) {
    return;
  }
  lastSendMs = now;

  ledSocket.cleanupClients();
  if (ledSocket.count() == 0) {
    return;
  }

  // Backpressure. Without this a stalled client silently queues up to
  // WS_MAX_QUEUED_MESSAGES (32 on ESP32) copies of the frame — 27 KB of heap
  // per client at this frame size. Dropping the frame is always the right
  // answer for a live preview.
  if (!ledSocket.availableForWriteAll()) {
    droppedFrames++;
    if (now - lastDropReportMs > 10000) {
      lastDropReportMs = now;
      Serial.printf("[WS] %lu frames dropped (slow client)\n",
                    (unsigned long)droppedFrames);
      droppedFrames = 0;
    }
    return;
  }

  // CRGB is exactly three bytes per LED in strip order, so the frame is the
  // FastLED buffer verbatim.
  static_assert(sizeof(CRGB) == 3, "CRGB must be 3 bytes to send it raw");
  ledSocket.binaryAll(reinterpret_cast<const uint8_t *>(leds),
                      NUM_LEDS * sizeof(CRGB));
}
