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

// Signal strength in dBm, or 0 when not associated. Roughly: -50 excellent,
// -70 usable, below -80 is where TCP starts falling apart while ping still works.
int networkRssi();
