
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

// Flag to prevent loopNetwork from running during mode switch
bool networkSwitching = false;

// Cleanly stop all network services
void stopNetworkServices() {
  networkSwitching = true;

  // Stop DNS server first
  dnsServer.stop();
  delay(10);

  // Stop mDNS
  MDNS.end();
  delay(200);

  // Disconnect WiFi
  WiFi.disconnect(true); // true = also erase stored credentials
  WiFi.mode(WIFI_OFF);
  delay(100);

  networkSwitching = false;
}

// Start Captive Portal mode
void startCaptivePortal() {
  Serial.println("  Starting Captive Portal...");

  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);

  MDNS.begin(HOSTNAME);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  activeNetworkMode = NETWORK_CAPTIVE;
  Serial.printf("  AP SSID: %s\n", AP_SSID);
  Serial.printf("  IP: %s\n", apIP.toString().c_str());
}

// Try to connect to WiFi in client mode
bool connectToWiFi() {
  if (strlen(networkSettings.ssid) == 0) {
    Serial.println("  No SSID configured");
    return false;
  }

  Serial.printf("  Connecting to: %s\n", networkSettings.ssid);

  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.begin(networkSettings.ssid, networkSettings.password);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > WIFI_CONNECT_TIMEOUT) {
      Serial.println("\n  Connection timeout!");
      return false;
    }
    delay(250);
    Serial.print(".");
  }

  Serial.printf("\n  Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  MDNS.begin(HOSTNAME);
  activeNetworkMode = NETWORK_CLIENT;
  return true;
}

void setupNetwork() {
  Serial.println("=== Network Setup ===");

  if (networkSettings.mode == NETWORK_CLIENT) {
    // Try to connect to configured WiFi
    if (!connectToWiFi()) {
      // Connection failed
      if (networkSettings.fallbackToCaptive) {
        Serial.println("  Falling back to Captive Portal");
        startCaptivePortal();
      } else {
        Serial.println("  WiFi connection failed, no fallback enabled");
        // Keep trying in client mode
        activeNetworkMode = NETWORK_CLIENT;
      }
    }
  } else {
    // Default: Captive Portal mode
    startCaptivePortal();
  }
  Serial.println("=====================");
}

void loopNetwork() {
  // Don't process during network mode switch
  if (networkSwitching) {
    return;
  }

  if (activeNetworkMode == NETWORK_CAPTIVE) {
    dnsServer.processNextRequest();
  } else if (activeNetworkMode == NETWORK_CLIENT) {
    // Check if still connected, try to reconnect if not
    static unsigned long lastReconnectAttempt = 0;
    if (WiFi.status() != WL_CONNECTED &&
        (millis() - lastReconnectAttempt > 5000)) {
      lastReconnectAttempt = millis();
      Serial.println("WiFi disconnected, reconnecting...");
      stopNetworkServices();
      if (!connectToWiFi() && networkSettings.fallbackToCaptive) {
        Serial.println("Reconnection failed, falling back to Captive Portal");
        startCaptivePortal();
      }
    }
  }
}

// Restart network with new settings (called after saving network config)
void restartNetwork() {
  Serial.println("\n>>> Restarting network with new settings...");
  stopNetworkServices();
  setupNetwork();
}