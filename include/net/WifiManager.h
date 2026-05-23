#pragma once

#include <Arduino.h>
#include <WiFi.h>

class WifiManager {
public:
  bool connect(const String& ssid, const String& password, uint32_t timeoutMs, bool forceReconnect = false);
  bool startProvisioning(const String& apSsid, const String& apPassword, String& outSsid, String& outPassword);
  bool isConnected() const;

  static const char* statusToText(int status);
};
