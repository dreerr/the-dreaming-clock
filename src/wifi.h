#include "definitions.h"
#include <DNSServer.h>
#include <ESP8266WiFiMulti.h>
//#include <WiFiManager.h>
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
ESP8266WiFiMulti WiFiMulti;
DNSServer dnsServer;

void setupWifi() {
  DEBUG.printf("Setup Wifi\n");
  // WiFiManager wifiManager;
  // wifiManager.autoConnect("AutoConnectAP");
  if(USE_CAPTIVE) {
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(SSID);
    MDNS.begin(HOSTNAME);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", apIP);
  } else {
    WiFiMulti.addAP("ssid", "pass");
    while(WiFiMulti.run() != WL_CONNECTED) {
      delay(100);
    }
  }
}

void loopWifi() { dnsServer.processNextRequest(); }