#include <Arduino.h>
#include <ESP8266WiFi.h>

#include <Time.h>
#include <TimeLib.h>

// #include <Wire.h>

#include "definitions.h"
#include "ota.h"
#include "wifi.h"
#include "webserver_async.h"
#include "leds.h"
#include "sensor.h"

// #include <DNSServer.h>

//#include <WiFiManager.h>

// #include <Thread.h>
// #include <ThreadController.h>

// ThreadController controll = ThreadController();
// Thread myThread = Thread();

void setup() {
  DEBUG.begin(115200);
  for(uint8_t t = 4; t > 0; t--) {
    DEBUG.printf("BOOTING %d...\n", t);
    DEBUG.flush();
    delay(1000);
  }



  DEBUG.printf("Setup Wifi\n");
  setupWifi();
  DEBUG.printf("Setup OTA\n");
  setupOTA();
  DEBUG.printf("Setup LEDs\n");
  setupLEDs();
  DEBUG.printf("Setup Webserver\n");
  setupWebserver();
  DEBUG.printf("Setup Sensor\n");
  setupSensor();
  // DEBUG.printf("Setup Websocket\n");
  // setupWebsocket();

  DEBUG.printf("Setup DONE!\n");
  // myThread.onRun(loopWebserver);
  // myThread.setInterval(10);
  // controll.add(&myThread);
}

char buffer[80];
void loop() {

  loopOTA(); // Don't delete!
  loopWifi(); // Don't delete!
  //loopWebserver();
  // loopWebsocket();
  loopLEDs();
  loopSensor();
  // controll.run();
}
