#pragma once
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// LED array from leds.h
extern CRGB leds[];
extern const int NUM_LEDS;
extern const int NUM_SEGMENTS;
extern const int LEDS_PER_SEGMENT;
extern const int COLON_LEDS;

// ============================================================================
// WebSocket LED Preview
// ============================================================================
// Provides real-time LED state streaming to web clients via WebSocket.
// Clients receive binary data with averaged RGB values per segment.
//
// Protocol:
//   - Endpoint: /ws/leds
//   - Format: Binary, 29 segments × 3 bytes (RGB) = 87 bytes
//   - Update rate: ~20 FPS (50ms interval)
//
// Segment layout:
//   - Segments 0-27: 4 digits × 7 segments each (10 LEDs per segment)
//   - Segment 28: Colon (2 LEDs at index 140-141)
// ============================================================================

AsyncWebSocket ledSocket("/ws/leds");

// WebSocket event handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  }
}

// Send LED data to all connected WebSocket clients
// Format: Binary data with 29 segments × 3 bytes (RGB average per segment)
void sendLedPreview() {
  if (ledSocket.count() == 0)
    return; // No clients connected

  // Send averaged RGB per segment (29 segments × 3 bytes = 87 bytes)
  uint8_t buffer[29 * 3];

  for (int seg = 0; seg < 28; seg++) {
    // Calculate start index for this segment
    int start = seg * 10;
    if (seg >= 14)
      start += 2; // Skip colon LEDs after segment 13

    // Average the 10 LEDs in this segment
    uint32_t r = 0, g = 0, b = 0;
    for (int i = 0; i < 10; i++) {
      r += leds[start + i].r;
      g += leds[start + i].g;
      b += leds[start + i].b;
    }
    buffer[seg * 3] = r / 10;
    buffer[seg * 3 + 1] = g / 10;
    buffer[seg * 3 + 2] = b / 10;
  }

  // Colon segment (segment 28, at LED index 140-141)
  buffer[28 * 3] = (leds[140].r + leds[141].r) / 2;
  buffer[28 * 3 + 1] = (leds[140].g + leds[141].g) / 2;
  buffer[28 * 3 + 2] = (leds[140].b + leds[141].b) / 2;

  ledSocket.binaryAll(buffer, sizeof(buffer));
}

// Register WebSocket handler with AsyncWebServer
void setupWebSocket(AsyncWebServer &server) {
  Serial.println("=== WebSocket Setup ===");
  ledSocket.onEvent(onWsEvent);
  server.addHandler(&ledSocket);
  Serial.println("  Endpoint: /ws/leds");
  Serial.println("  Protocol: Binary (87 bytes per frame)");
  Serial.println("  Update rate: ~20 FPS");
  Serial.println("=======================\n");
}

// Call this from the main loop to send LED updates
void loopWebSocket() {
  static unsigned long lastWsUpdate = 0;
  if (millis() - lastWsUpdate > 50) { // ~20 FPS for WebSocket
    lastWsUpdate = millis();
    sendLedPreview();
    ledSocket.cleanupClients();
  }
}
