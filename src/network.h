
#pragma once
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "settings.h"

// DNS Server for Captive Portal
const int DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

// Connection timeout for client mode (ms)
#define WIFI_CONNECT_TIMEOUT 15000

// Track current active mode (may differ from settings if fallback occurred)
NetworkMode activeNetworkMode = NETWORK_CAPTIVE;

// Start Captive Portal mode
void startCaptivePortal() {
  Serial.println("Starting Captive Portal...");
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);
  MDNS.begin(HOSTNAME);
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  activeNetworkMode = NETWORK_CAPTIVE;
  Serial.printf("Captive Portal started: %s (IP: %s)\n", AP_SSID,
                apIP.toString().c_str());
}

// Try to connect to WiFi in client mode
bool connectToWiFi() {
  if (strlen(networkSettings.ssid) == 0) {
    Serial.println("No SSID configured");
    return false;
  }

  Serial.printf("Connecting to WiFi: %s\n", networkSettings.ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(networkSettings.ssid, networkSettings.password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println("WiFi connection timeout");
      return false;
    }
    delay(100);
    Serial.print(".");
  }

  Serial.printf("\nConnected to %s (IP: %s)\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str());
  MDNS.begin(HOSTNAME);
  activeNetworkMode = NETWORK_CLIENT;
  return true;
}

void setupNetwork() {
  Serial.println("Setup Network");

  if (networkSettings.mode == NETWORK_CLIENT) {
    // Try to connect to configured WiFi
    if (!connectToWiFi()) {
      // Connection failed
      if (networkSettings.fallbackToCaptive) {
        Serial.println("Falling back to Captive Portal");
        startCaptivePortal();
      } else {
        Serial.println("WiFi connection failed, no fallback enabled");
        // Keep trying in client mode
        activeNetworkMode = NETWORK_CLIENT;
      }
    }
  } else {
    // Default: Captive Portal mode
    startCaptivePortal();
  }
}

void loopNetwork() {
  if (activeNetworkMode == NETWORK_CAPTIVE) {
    dnsServer.processNextRequest();
  } else if (activeNetworkMode == NETWORK_CLIENT) {
    // Check if still connected, try to reconnect if not
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      if (!connectToWiFi() && networkSettings.fallbackToCaptive) {
        Serial.println("Reconnection failed, falling back to Captive Portal");
        startCaptivePortal();
      }
    }
  }
}

// Restart network with new settings (called after saving network config)
void restartNetwork() {
  Serial.println("Restarting network with new settings...");
  dnsServer.stop();
  WiFi.disconnect();
  delay(100);
  setupNetwork();
}