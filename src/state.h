#pragma once
#include <ArduinoJson.h>

// ===========================================================================
// Canonical device state
// ===========================================================================
//
// One serializer and one command entry point. Every transport — the REST API
// today, MQTT/Home Assistant later — reads and writes through these two
// functions, so they cannot drift apart and validation lives in exactly one
// place.

void serializeState(JsonObject out);

struct CommandResult {
  bool ok;
  const char *message;
};

// Apply a single setting. Validates, persists to NVS, and runs any side effect
// (re-applying the timezone, rescheduling the wakeup, restarting the network).
CommandResult applyCommand(const char *key, JsonVariantConst value);

// Apply every key in the object. Stops at the first failure and reports it.
CommandResult applyCommands(JsonObjectConst commands);
