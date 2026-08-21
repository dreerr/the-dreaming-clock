#include "net_wifi.h"

#include <DNSServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include "config.h"

namespace {

constexpr int DNS_PORT = 53;
constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t RETRY_INTERVAL_MS = 30000;

const IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;

enum class Phase : uint8_t { CAPTIVE, CONNECTING, CLIENT };

Phase phase = Phase::CAPTIVE;
NetworkMode active = NETWORK_CAPTIVE;
bool restartRequested = false;
bool mdnsUp = false;
uint32_t connectDeadline = 0;
uint32_t nextRetryAt = 0;

void startMdns() {
  if (mdnsUp) {
    MDNS.end();
    mdnsUp = false;
  }
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ota", "tcp", 3232);
    mdnsUp = true;
    Serial.printf("[NET] mDNS: http://%s.local\n", HOSTNAME);
  }
}

void startCaptivePortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);

  startMdns();
  phase = Phase::CAPTIVE;
  active = NETWORK_CAPTIVE;
  Serial.printf("[NET] Captive portal \"%s\" at %s\n", AP_SSID,
                apIP.toString().c_str());
}

// Kicks off an association attempt and returns immediately. The result arrives
// as a WiFi event; nothing here blocks the main loop.
void startClient() {
  if (strlen(networkSettings.ssid) == 0) {
    Serial.println(F("[NET] Client mode requested but no SSID configured"));
    startCaptivePortal();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(networkSettings.ssid, networkSettings.password);

  phase = Phase::CONNECTING;
  active = NETWORK_CLIENT;
  connectDeadline = millis() + CONNECT_TIMEOUT_MS;
  Serial.printf("[NET] Connecting to \"%s\"...\n", networkSettings.ssid);
}

void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    phase = Phase::CLIENT;
    active = NETWORK_CLIENT;
    Serial.printf("[NET] Connected, IP %s\n", WiFi.localIP().toString().c_str());
    startMdns();
    break;

  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
    if (phase == Phase::CLIENT) {
      Serial.println(F("[NET] Disconnected, auto-reconnecting"));
      phase = Phase::CONNECTING;
      connectDeadline = millis() + CONNECT_TIMEOUT_MS;
    }
    break;

  default:
    break;
  }
}

void stopServices() {
  dnsServer.stop();
  if (mdnsUp) {
    MDNS.end();
    mdnsUp = false;
  }
  WiFi.disconnect(false, false); // keep the radio on, keep stored credentials
  WiFi.softAPdisconnect(true);
}

} // namespace

void setupNetwork() {
  Serial.println(F("=== Network ==="));
  WiFi.onEvent(onWiFiEvent);
  WiFi.persistent(false);

  if (networkSettings.mode == NETWORK_CLIENT) {
    startClient();
  } else {
    startCaptivePortal();
  }
  Serial.println(F("===============\n"));
}

void loopNetwork() {
  if (restartRequested) {
    restartRequested = false;
    Serial.println(F("[NET] Applying new network settings"));
    stopServices();
    if (networkSettings.mode == NETWORK_CLIENT) {
      startClient();
    } else {
      startCaptivePortal();
    }
    return;
  }

  if (phase == Phase::CAPTIVE) {
    dnsServer.processNextRequest();
    return;
  }

  if (phase == Phase::CONNECTING && millis() > connectDeadline) {
    if (networkSettings.fallbackToCaptive) {
      Serial.println(F("[NET] Connection timed out, falling back to captive"));
      stopServices();
      startCaptivePortal();
    } else if (millis() > nextRetryAt) {
      nextRetryAt = millis() + RETRY_INTERVAL_MS;
      connectDeadline = millis() + CONNECT_TIMEOUT_MS;
      Serial.println(F("[NET] Connection timed out, retrying"));
      WiFi.reconnect();
    }
  }
}

void requestNetworkRestart() { restartRequested = true; }

NetworkMode activeNetworkMode() { return active; }

bool networkConnected() {
  return phase == Phase::CLIENT && WiFi.status() == WL_CONNECTED;
}

String networkAddress() {
  if (phase == Phase::CAPTIVE) {
    return apIP.toString();
  }
  if (networkConnected()) {
    return WiFi.localIP().toString();
  }
  return String();
}
