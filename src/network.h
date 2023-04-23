#include "definitions.h"
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>

#include <DNSServer.h>

const int DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
ESP8266WiFiMulti WiFiMulti;
DNSServer dnsServer;

void setupNetwork() {
  DEBUG.println("Setup Network");

  if (USE_CAPTIVE) {
    DEBUG.println("Setup Network: Captive");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(AP_SSID);
    MDNS.begin(HOSTNAME);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
  } else {
    WiFiMulti.addAP("unit.network.legacy", "Hallo1020Freund!");
    WiFiMulti.addAP("phone unit.", "immerwieder23");
    while (WiFiMulti.run() != WL_CONNECTED) {
      delay(100);
    }
    Serial.printf(" connected to %s\n", WiFi.SSID().c_str());
  }
}

void loopNetwork() {
  if (USE_CAPTIVE) {
    dnsServer.processNextRequest();
  }
}