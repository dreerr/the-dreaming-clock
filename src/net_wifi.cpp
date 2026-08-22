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

// Tears the responder down and rebuilds it from scratch every time. That is
// deliberate: ArduinoOTA.begin() *also* calls MDNS.begin() behind our back, so a
// local "is it up?" flag cannot describe the responder's real state, and the
// order the two run in depends on how fast the AP hands out a lease. MDNS.end()
// is a no-op on a responder that was never started, so an unconditional
// rebuild is the only version that is correct in both orders.
void startMdns() {
  MDNS.end();
  mdnsUp = false;

  if (!MDNS.begin(HOSTNAME)) {
    Serial.println(F("[NET] mDNS failed to start"));
    return;
  }
  MDNS.addService("http", "tcp", 80);

  // Not addService("ota", ...): espota and PlatformIO discover the board by
  // _arduino._tcp, which is what enableArduino() publishes. ArduinoOTA
  // registers it once at boot and never again, so the MDNS.end() above would
  // otherwise leave OTA undiscoverable after the first reconnect.
  MDNS.enableArduino(3232, true);

  mdnsUp = true;
  Serial.printf("[NET] mDNS: http://%s.local\n", HOSTNAME);
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

  // Modem sleep (the Arduino default in STA mode) parks the radio between the
  // AP's DTIM beacons. ICMP survives that happily, which is why a router with a
  // long DTIM interval still pings perfectly while HTTP crawls: every TCP
  // handshake and every ACK waits for the next beacon. The clock runs off mains
  // power, so the ~80 mA this costs buys a responsive web UI on any router.
  WiFi.setSleep(false);

  WiFi.setAutoReconnect(true);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(networkSettings.ssid, networkSettings.password);

  phase = Phase::CONNECTING;
  active = NETWORK_CLIENT;
  connectDeadline = millis() + CONNECT_TIMEOUT_MS;
  Serial.printf("[NET] Connecting to \"%s\"...\n", networkSettings.ssid);
}

// The reason codes worth recognising when a board that was fine on one network
// misbehaves on the next. Anything else prints as a bare number; the full list
// is wifi_err_reason_t in esp_wifi_types.h.
const char *disconnectReason(uint8_t reason) {
  switch (reason) {
  case 2:
    return "auth expired";
  case 4:
    return "association expired";
  case 15:
    return "4-way handshake timeout (wrong password?)";
  case 200:
    return "beacon timeout (weak signal)";
  case 201:
    return "no AP found";
  case 202:
    return "auth failed";
  case 203:
    return "association failed";
  case 204:
    return "handshake timeout";
  default:
    return nullptr;
  }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
  case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    phase = Phase::CLIENT;
    active = NETWORK_CLIENT;
    // RSSI and channel are the first two things worth knowing when the same
    // firmware behaves differently on a different router.
    Serial.printf("[NET] Connected, IP %s (RSSI %d dBm, channel %d)\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(),
                  WiFi.channel());
    startMdns();
    break;

  case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
    const uint8_t reason = info.wifi_sta_disconnected.reason;
    const char *name = disconnectReason(reason);
    if (name != nullptr) {
      Serial.printf("[NET] Disconnected: %s (%u)\n", name, reason);
    } else {
      Serial.printf("[NET] Disconnected: reason %u\n", reason);
    }
    if (phase == Phase::CLIENT) {
      phase = Phase::CONNECTING;
      connectDeadline = millis() + CONNECT_TIMEOUT_MS;
    }
    break;
  }

  default:
    break;
  }
}

void stopServices() {
  dnsServer.stop();
  MDNS.end(); // unconditional for the same reason startMdns() rebuilds: the
  mdnsUp = false; // responder may have been started by ArduinoOTA, not by us
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

int networkRssi() {
  return networkConnected() ? WiFi.RSSI() : 0;
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
