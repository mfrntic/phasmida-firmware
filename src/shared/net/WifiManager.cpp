#include <net/WifiManager.h>
#include <WiFiManager.h>
#include <app_config.h>

bool WifiManager::connect(const String& ssid, const String& password, uint32_t timeoutMs, bool forceReconnect) {
  if (ssid.isEmpty()) {
    return false;
  }

  if (!forceReconnect && WiFi.status() == WL_CONNECTED) {
    return true;
  }

  if (forceReconnect) {
    Serial.printf("[WIFI] Forcing station reconnect (status=%s, ip=%s, dns0=%s, dns1=%s)\n",
                  statusToText(WiFi.status()),
                  WiFi.localIP().toString().c_str(),
                  WiFi.dnsIP(0).toString().c_str(),
                  WiFi.dnsIP(1).toString().c_str());
  }

  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  delay(250);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  uint32_t startAt = millis();
  int attempt = 0;
  int reconnectTries = 0;
  while (WiFi.status() != WL_CONNECTED && (millis() - startAt) < timeoutMs) {
    delay(1000);
    attempt++;
    int status = WiFi.status();
    if (status == WL_CONNECT_FAILED && reconnectTries < 3) {
      reconnectTries++;
      WiFi.disconnect(false, false);
      delay(200);
      WiFi.begin(ssid.c_str(), password.c_str());
    }
  }

  return WiFi.status() == WL_CONNECTED;
}

bool WifiManager::startProvisioning(const String& apSsid, const String& apPassword, String& outSsid, String& outPassword) {
  outSsid = "";
  outPassword = "";

  WiFi.persistent(false);
  WiFi.setSleep(false);
  WiFi.disconnect(true, true);
  delay(250);

  ::WiFiManager wm;
  wm.setConfigPortalBlocking(true);
  wm.setBreakAfterConfig(true);
  wm.setConnectTimeout(AppConfig::kWifiConnectTimeoutMs / 1000);
  wm.setConfigPortalTimeout(0);
  wm.setWebServerCallback([&wm]() {
    if (wm.server) {
      wm.server->on("/", HTTP_GET, [&wm]() {
        wm.server->sendHeader("Location", "/wifi", true);
        wm.server->send(302, "text/plain", "");
      });
    }
  });

  bool configured = apPassword.isEmpty()
    ? wm.startConfigPortal(apSsid.c_str())
    : wm.startConfigPortal(apSsid.c_str(), apPassword.c_str());
  if (!configured) {
    return false;
  }

  outSsid = wm.getWiFiSSID();
  outPassword = wm.getWiFiPass();
  return !outSsid.isEmpty();
}

bool WifiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

const char* WifiManager::statusToText(int status) {
  switch (status) {
    case WL_IDLE_STATUS:      return "IDLE";
    case WL_NO_SSID_AVAIL:    return "SSID_NOT_FOUND";
    case WL_SCAN_COMPLETED:   return "SCAN_COMPLETED";
    case WL_CONNECTED:        return "CONNECTED";
    case WL_CONNECT_FAILED:   return "CONNECT_FAILED";
    case WL_CONNECTION_LOST:  return "CONNECTION_LOST";
    case WL_DISCONNECTED:     return "DISCONNECTED";
    default:                   return "UNKNOWN";
  }
}
