#pragma once
#include <ESPAsyncWebServer.h>

// ===========================================================================
// WebSocket LED preview
// ===========================================================================
//
// Endpoint:  /ws/leds
// Format:    binary, NUM_LEDS * 3 bytes (846), one RGB triple per LED, in
//            strip order. No header — the length identifies the frame.
// Rate:      PREVIEW_INTERVAL_MS (25 FPS)
//
// Full per-LED fidelity costs ~21 KB/s and is cheaper on the ESP than the old
// per-segment averaging (a memcpy instead of 280 adds and 28 divides). Clients
// that cannot afford to draw 282 points can average locally; the choice of
// renderer is theirs, so there is nothing to negotiate.
//
// Text commands accepted from the client:
//   "calibrate on" / "calibrate off"

void setupWebSocket(AsyncWebServer &server);
void loopWebSocket();
