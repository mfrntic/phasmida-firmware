#include <ConfigStore.h>
#include <app_config.h>

bool ConfigStore::begin() {
  _open = _prefs.begin(AppConfig::kNvsNamespace, false);
  return _open;
}

bool ConfigStore::hasWifiCredentials() {
  if (!_open) {
    return false;
  }
  return !_prefs.getString(AppConfig::kNvsWifiSsid, "").isEmpty();
}

void ConfigStore::loadDefaults() {
  if (!_open) {
    return;
  }

  if (_prefs.getString(AppConfig::kNvsMqttHost, "").isEmpty()) {
    _prefs.putString(AppConfig::kNvsMqttHost, AppConfig::kDefaultMqttHost);
  }
  if (_prefs.getUShort(AppConfig::kNvsMqttPort, 0) == 0) {
    _prefs.putUShort(AppConfig::kNvsMqttPort, AppConfig::kMqttDefaultPort);
  }
  if (_prefs.getUInt(AppConfig::kNvsTelemetryInterval, 0) == 0) {
    _prefs.putUInt(AppConfig::kNvsTelemetryInterval, AppConfig::kTelemetryIntervalMs);
  }

  // Migrate old/incorrect broker settings.
  String mqttHost = _prefs.getString(AppConfig::kNvsMqttHost, "");
  uint16_t mqttPort = _prefs.getUShort(AppConfig::kNvsMqttPort, 0);
  if (mqttHost == "10.0.2.2") {
    _prefs.putString(AppConfig::kNvsMqttHost, AppConfig::kDefaultMqttHost);
    _prefs.putUShort(AppConfig::kNvsMqttPort, AppConfig::kMqttDefaultPort);
  }
}

RuntimeConfig ConfigStore::load() {
  RuntimeConfig cfg;
  cfg.wifiSsid             = _prefs.getString(AppConfig::kNvsWifiSsid,          "");
  cfg.wifiPassword         = _prefs.getString(AppConfig::kNvsWifiPass,           "");
  cfg.mqttHost             = _prefs.getString(AppConfig::kNvsMqttHost,           AppConfig::kDefaultMqttHost);
  cfg.mqttPort             = _prefs.getUShort(AppConfig::kNvsMqttPort,           AppConfig::kMqttDefaultPort);
  cfg.telemetryIntervalMs  = _prefs.getUInt(  AppConfig::kNvsTelemetryInterval,  AppConfig::kTelemetryIntervalMs);

  if (cfg.mqttHost.isEmpty())     cfg.mqttHost     = AppConfig::kDefaultMqttHost;
  if (cfg.mqttPort == 0)          cfg.mqttPort     = AppConfig::kMqttDefaultPort;

  cfg.telemetryIntervalMs = max(cfg.telemetryIntervalMs, AppConfig::kMinTelemetryIntervalMs);

  return cfg;
}

void ConfigStore::setTelemetryInterval(uint32_t ms) {
  if (_open) {
    _prefs.putUInt(AppConfig::kNvsTelemetryInterval, ms);
  }
}

void ConfigStore::setWifi(const String& ssid, const String& pass) {
  if (_open) {
    _prefs.putString(AppConfig::kNvsWifiSsid, ssid);
    _prefs.putString(AppConfig::kNvsWifiPass, pass);
  }
}

void ConfigStore::setMqttBroker(const String& host, uint16_t port) {
  if (_open) {
    _prefs.putString(AppConfig::kNvsMqttHost, host);
    _prefs.putUShort(AppConfig::kNvsMqttPort, port);
  }
}

void ConfigStore::clear() {
  if (_open) {
    _prefs.clear();
  }
}

void ConfigStore::saveLightState(CRGB color, uint8_t brightness, const String& cmdId) {
  if (!_open) return;
  uint32_t packed = ((uint32_t)color.r << 16) | ((uint32_t)color.g << 8) | color.b;
  _prefs.putUInt(AppConfig::kNvsLightColor, packed);
  _prefs.putUChar(AppConfig::kNvsLightBrightness, brightness);
  _prefs.putString(AppConfig::kNvsLightCmdId, cmdId);
}

void ConfigStore::clearLightState() {
  if (!_open) return;
  _prefs.remove(AppConfig::kNvsLightColor);
  _prefs.remove(AppConfig::kNvsLightBrightness);
  _prefs.remove(AppConfig::kNvsLightCmdId);
}

PersistedLightState ConfigStore::loadLightState() {
  PersistedLightState s;
  if (!_open) return s;
  // Key presence check: if color key is missing, no state was ever saved
  if (!_prefs.isKey(AppConfig::kNvsLightColor)) return s;
  uint32_t packed = _prefs.getUInt(AppConfig::kNvsLightColor, 0);
  s.color         = CRGB((packed >> 16) & 0xFF, (packed >> 8) & 0xFF, packed & 0xFF);
  s.brightness    = _prefs.getUChar(AppConfig::kNvsLightBrightness, 0);
  s.cmdId         = _prefs.getString(AppConfig::kNvsLightCmdId, "");
  s.valid         = true;
  return s;
}
