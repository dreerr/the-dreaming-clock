#include "definitions.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include <DNSServer.h>

const int DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
WiFiMulti WiFiMulti;
DNSServer dnsServer;

void setupNetwork() {
  Serial.println("Setup Network");

  if (USE_CAPTIVE) {
    Serial.println("Setup Network: Captive");
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