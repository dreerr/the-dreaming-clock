#pragma once
#include <ESPAsyncWebServer.h>

// ===========================================================================
// WebSocket LED preview
// ===========================================================================
//
// Endpoint:  /ws/leds
// Format:    binary, NUM_LEDS * 3 bytes, one RGB triple per LED in strip
//            order. No header — GET /api/layout describes the mapping, so the
//            frame is pure pixel data and the count is not baked into clients.
// Rate:      PREVIEW_INTERVAL_MS (25 FPS)
//
// Full per-LED fidelity is cheaper on the ESP than the old per-segment
// averaging (a memcpy instead of a divide per segment). Clients that cannot
// afford to draw every LED can average locally using the ranges from
// /api/layout; the choice of renderer is theirs, so there is nothing to
// negotiate.
//
// Text commands accepted from the client:
//   "calibrate on" / "calibrate off"

void setupWebSocket(AsyncWebServer &server);
void loopWebSocket();
