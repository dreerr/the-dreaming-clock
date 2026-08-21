#include <Arduino.h>

#include "clock_time.h"
#include "config.h"
#include "dreams.h"
#include "leds.h"
#include "modes.h"
#include "net_wifi.h"
#include "ota.h"
#include "settings.h"
#include "web.h"

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println(F("╔══════════════════════════════════════╗"));
  Serial.println(F("║     THE DREAMING CLOCK - ESP32-C3    ║"));
  Serial.println(F("╚══════════════════════════════════════╝"));
  Serial.println();

  // Seed both RNGs from hardware entropy. Without this the "random" dream and
  // colour sequences were identical on every boot.
  const uint32_t seed = esp_random();
  randomSeed(seed);
  seedDreamWords(seed);

  setupSettings();
  setupNetwork();
  setupClockTime();
  setupOTA();
  setupWeb();
  setupLEDs();

  Serial.printf("Setup complete. Free heap: %u bytes\n", ESP.getFreeHeap());
  Serial.println();
}

void loop() {
  loopNetwork();
  loopOTA();
  loopClockTime();
  loopWeb();
  loopLEDs();
}
