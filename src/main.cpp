#include "settings.h"
#include <Arduino.h>

// Globale Variablen (deklariert in settings.h)
bool wakeup = false;
bool timeWasSet = false;

#include "leds.h"
#include "network.h"
#include "ota.h"
#include "rtc.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("BOOTING %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  setupRTC();
  setupSettings();
  setupNetwork();
  setupOTA();
  setupWeb();
  setupLEDs();
  Serial.println("Setup finished!");
}

void loop() {
  loopNetwork(); // Don't delete!
  loopOTA();     // Don't delete!
  loopLEDs();
}
