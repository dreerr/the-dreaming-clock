#pragma once
#include <Arduino.h>

#include "settings.h"

void setupNetwork();
void loopNetwork();

// Apply new network settings. Safe to call from a web handler: the actual
// teardown and reconnect happen on the next loop() pass, never inside the
// AsyncTCP task.
void requestNetworkRestart();

NetworkMode activeNetworkMode();
bool networkConnected();
String networkAddress();
