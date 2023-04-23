#pragma once
#define AP_SSID "the dreaming clock"
#define HOSTNAME "the-dreaming-clock"
#define HTTPHOST "http://the-dreaming-clock.local"
#define DEBUG Serial
#define USE_CAPTIVE true
#define HOUR_START 8
#define HOUR_END 18
#define ONLY_OFFICE_HOURS false

bool wakeup = false;
bool timeWasSet = true;