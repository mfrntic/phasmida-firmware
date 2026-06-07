#pragma once

#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>

struct RuntimeConfig {
  String   wifiSsid;
  String   wifiPassword;
  String   mqttHost;
  uint16_t mqttPort;
  uint32_t telemetryIntervalMs;
};

struct PersistedLightState {
  bool     valid      = false;
  CRGB     color      = CRGB::Black;
  uint8_t  brightness = 0;
  String   cmdId;
};

class ConfigStore {
public:
  bool begin();                                          // otvori NVS namespace; false = koristit će compile-time defaulte
  bool hasWifiCredentials();
  void loadDefaults();                                   // upiši defaulte ako prazno; migrira stare vrijednosti (idempotentno)
  RuntimeConfig load();                            // čitaj sve; uvijek vraća valjanu config

  void setTelemetryInterval(uint32_t ms);
  void setWifi(const String& ssid, const String& pass);
  void setMqttBroker(const String& host, uint16_t port);
  void setTimezone(const String& posixTz);
  String loadTimezone();
  bool hasTimezoneConfigured();
  void clear();                                          // factory-reset: obriši sve NVS ključeve u namespaceu

  // RGB set-light persistence
  void saveLightState(CRGB color, uint8_t brightness, const String& cmdId);
  void clearLightState();
  PersistedLightState loadLightState();
  void setLightLocalOverride(bool enabled);
  bool isLightLocalOverrideActive();

private:
  Preferences _prefs;
  bool        _open = false;
};
