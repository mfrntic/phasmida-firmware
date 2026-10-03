#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  bool connect(const String& ssid, const String& password, uint32_t timeoutMs, bool forceReconnect = false);
  // portalTimeoutSec = 0 keeps the portal open until credentials are saved.
  bool startProvisioning(const String& apSsid, const String& apPassword, String& outSsid, String& outPassword,
                         uint32_t portalTimeoutSec = 0);
  bool isConnected() const;

  static const char* statusToText(int status);
};
