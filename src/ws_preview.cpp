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

// Per-client "last accepted a frame" tracking, used to evict sockets that are
// open but permanently full.
constexpr size_t kMaxTrackedClients = 8;
constexpr uint32_t kStallTimeoutMs = 5000;

struct ClientHealth {
  uint32_t id = 0;
  uint32_t lastOkMs = 0;
};
ClientHealth clientHealth[kMaxTrackedClients];

ClientHealth *slotFor(uint32_t id) {
  ClientHealth *free = nullptr;
  for (auto &slot : clientHealth) {
    if (slot.id == id) {
      return &slot;
    }
    if (slot.id == 0 && free == nullptr) {
      free = &slot;
    }
  }
  return free;
}

void markHealthy(uint32_t id, uint32_t now) {
  ClientHealth *slot = slotFor(id);
  if (slot != nullptr) {
    slot->id = id;
    slot->lastOkMs = now;
  }
}

bool stalledTooLong(uint32_t id, uint32_t now) {
  ClientHealth *slot = slotFor(id);
  if (slot == nullptr) {
    return false;
  }
  if (slot->id != id) { // first time we have seen it, and it is already full
    slot->id = id;
    slot->lastOkMs = now;
    return false;
  }
  return (now - slot->lastOkMs) > kStallTimeoutMs;
}

void forgetClient(uint32_t id) {
  for (auto &slot : clientHealth) {
    if (slot.id == id) {
      slot.id = 0;
    }
  }
}

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
    forgetClient(client->id());
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

  // CRGB is exactly three bytes per LED in strip order, so the frame is the
  // FastLED buffer verbatim.
  static_assert(sizeof(CRGB) == 3, "CRGB must be 3 bytes to send it raw");
  const uint8_t *frame = reinterpret_cast<const uint8_t *>(leds);
  constexpr size_t frameLen = NUM_LEDS * sizeof(CRGB);

  // Sending is per-client on purpose. availableForWriteAll() is all-or-nothing
  // — it reports false if *any* client's queue is full — so a single stalled
  // viewer (a backgrounded tab, a half-closed socket) would starve everyone
  // else. Checking each client individually keeps the healthy ones streaming.
  for (auto &client : ledSocket.getClients()) {
    if (client.status() != WS_CONNECTED) {
      continue;
    }
    if (client.canSend()) {
      client.binary(frame, frameLen);
      markHealthy(client.id(), now);
      continue;
    }

    // A full queue is normal for a moment; dropping the frame is always right
    // for a live preview. Staying full is a client that is never coming back,
    // and it would otherwise hold WS_MAX_QUEUED_MESSAGES × 846 bytes of heap
    // indefinitely.
    droppedFrames++;
    if (stalledTooLong(client.id(), now)) {
      Serial.printf("[WS] Client #%u stalled, closing\n", client.id());
      client.close();
      forgetClient(client.id());
    }
  }

  if (droppedFrames > 0 && now - lastDropReportMs > 10000) {
    lastDropReportMs = now;
    Serial.printf("[WS] %lu frames dropped (slow client)\n",
                  (unsigned long)droppedFrames);
    droppedFrames = 0;
  }
}
