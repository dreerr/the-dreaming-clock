#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "definitions.h"
#include "ota.h"
#include "wifi.h"
#include "webserver_async.h"
#include "leds.h"

void setup() {
  DEBUG.begin(115200);
  for(uint8_t t = 4; t > 0; t--) {
    DEBUG.printf("BOOTING %d...\n", t);
    DEBUG.flush();
    delay(1000);
  }
  setupWifi();
  setupOTA();
  setupLEDs();
  setupWebserver();
}

void loop() {
  loopWifi(); // Don't delete!
  loopOTA(); // Don't delete!
  loopLEDs();
}
