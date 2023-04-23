#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "definitions.h"
#include "leds.h"
#include "network.h"
#include "ota.h"
#include "web.h"

void setup() {
  DEBUG.begin(115200);
  for (uint8_t t = 4; t > 0; t--) {
    DEBUG.printf("BOOTING %d...\n", t);
    DEBUG.flush();
    delay(1000);
  }
  setupNetwork();
  setupOTA();
  setupWeb();
  setupLEDs();
  DEBUG.println("Setup finished!");
}

void loop() {
  loopNetwork(); // Don't delete!
  loopOTA();     // Don't delete!
  loopLEDs();
}
