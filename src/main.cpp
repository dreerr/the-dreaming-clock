#include <Arduino.h>

#include "settings.h"

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
  delay(100); // Give Serial time to initialize

  Serial.println();
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║     THE DREAMING CLOCK - ESP32-C3    ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.println();

  for (uint8_t t = 3; t > 0; t--) {
    Serial.printf("Starting in %d...\n", t);
    Serial.flush();
    delay(1000);
  }
  Serial.println();

  setupRTC();
  setupSettings();
  setupNetwork();
  setupOTA();
  setupWeb();
  setupLEDs();

  Serial.println();
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║         SETUP COMPLETE! ✓            ║");
  Serial.println("╚══════════════════════════════════════╝");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.println();
}

void loop() {
  loopNetwork(); // Don't delete!
  loopOTA();     // Don't delete!
  loopWeb();     // WebSocket LED preview
  loopLEDs();
}
