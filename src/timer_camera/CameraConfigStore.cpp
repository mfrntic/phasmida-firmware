#include <CameraConfigStore.h>
#include <camera_config.h>

bool CameraConfigStore::begin() {
  _open = _prefs.begin(CamConfig::kNvsNamespace, false);
  if (_open) {
    // Migration v1: seed quality defaults once.
    // Migration v2: fix wrong historical FRAMESIZE mapping where value 5 was treated as VGA.
    // Migration v3: raise default JPEG quality to 60 for devices still on old defaults.
    // Migration v4: raise default frame size to SVGA (800x600).
    // Migration v5: fix JPEG scale misunderstanding (lower value means higher quality).
    // Migration v6: seed OV3660 tuning defaults to NVS.
    uint8_t qualityVer = _prefs.getUChar(CamConfig::kNvsQualityVer, 0);
    if (qualityVer < 1) {
      _prefs.putUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
      _prefs.putUChar(CamConfig::kNvsFrameSize,   CamConfig::kDefaultFrameSize);
      _prefs.putUChar(CamConfig::kNvsQualityVer,  1);
      Serial.printf("[CameraConfigStore] Migration v1: quality=%u frameSize=%u\n",
                    CamConfig::kDefaultJpegQuality,
                    CamConfig::kDefaultFrameSize);
    }

    if (qualityVer < 2) {
      uint8_t storedFrameSize = _prefs.getUChar(CamConfig::kNvsFrameSize, CamConfig::kDefaultFrameSize);
      if (storedFrameSize == 5) {
        _prefs.putUChar(CamConfig::kNvsFrameSize, static_cast<uint8_t>(FRAMESIZE_VGA));
        Serial.printf("[CameraConfigStore] Migration v2: frameSize corrected %u -> %u (VGA)\n",
                      storedFrameSize,
                      static_cast<uint8_t>(FRAMESIZE_VGA));
      }
      _prefs.putUChar(CamConfig::kNvsQualityVer, 2);
    }

    if (qualityVer < 3) {
      uint8_t storedQuality = _prefs.getUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
      if (storedQuality == 20 || storedQuality == 50) {
        _prefs.putUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
        Serial.printf("[CameraConfigStore] Migration v3: jpeg quality corrected %u -> %u\n",
                      storedQuality,
                      CamConfig::kDefaultJpegQuality);
      }
      _prefs.putUChar(CamConfig::kNvsQualityVer, 3);
    }

    if (qualityVer < 4) {
      uint8_t storedFrameSize = _prefs.getUChar(CamConfig::kNvsFrameSize, CamConfig::kDefaultFrameSize);
      if (storedFrameSize == 8) {
        _prefs.putUChar(CamConfig::kNvsFrameSize, CamConfig::kDefaultFrameSize);
        Serial.printf("[CameraConfigStore] Migration v4: frameSize corrected %u -> %u (SVGA)\n",
                      storedFrameSize,
                      CamConfig::kDefaultFrameSize);
      }
      _prefs.putUChar(CamConfig::kNvsQualityVer, 4);
    }

    if (qualityVer < 5) {
      uint8_t storedQuality = _prefs.getUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
      if (storedQuality >= 45) {
        _prefs.putUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
        Serial.printf("[CameraConfigStore] Migration v5: jpeg quality corrected %u -> %u (lower=better)\n",
                      storedQuality,
                      CamConfig::kDefaultJpegQuality);
      }
      _prefs.putUChar(CamConfig::kNvsQualityVer, 5);
    }

    if (qualityVer < 6) {
      _prefs.putChar(CamConfig::kNvsSharpness, CamConfig::kDefaultSharpness);
      _prefs.putUChar(CamConfig::kNvsDenoise, CamConfig::kDefaultDenoise);
      _prefs.putUChar(CamConfig::kNvsLenc, CamConfig::kDefaultLenc);
      _prefs.putUChar(CamConfig::kNvsRawGma, CamConfig::kDefaultRawGma);
      _prefs.putUChar(CamConfig::kNvsAec2, CamConfig::kDefaultAec2);
      _prefs.putUChar(CamConfig::kNvsWpc, CamConfig::kDefaultWpc);
      _prefs.putUChar(CamConfig::kNvsBpc, CamConfig::kDefaultBpc);
      _prefs.putUChar(CamConfig::kNvsGainCeiling, CamConfig::kDefaultGainCeiling);
      _prefs.putUChar(CamConfig::kNvsQualityVer, 6);
      Serial.printf("[CameraConfigStore] Migration v6: OV3660 tuning defaults seeded (sharpness=%d denoise=%u lenc=%u rawGma=%u aec2=%u wpc=%u bpc=%u gainCeiling=%u)\n",
                    CamConfig::kDefaultSharpness,
                    CamConfig::kDefaultDenoise,
                    CamConfig::kDefaultLenc,
                    CamConfig::kDefaultRawGma,
                    CamConfig::kDefaultAec2,
                    CamConfig::kDefaultWpc,
                    CamConfig::kDefaultBpc,
                    CamConfig::kDefaultGainCeiling);
    }
  }
  return _open;
}

bool CameraConfigStore::hasWifiCredentials() const {
  if (!_open) return false;
  return _prefs.isKey(CamConfig::kNvsWifiSsid) &&
         !_prefs.getString(CamConfig::kNvsWifiSsid, "").isEmpty();
}

CameraConfig CameraConfigStore::load() const {
  CameraConfig cfg;
  cfg.mqttHost = String(CamConfig::kDefaultMqttHost);
  cfg.mqttPort = CamConfig::kDefaultMqttPort;
  if (!_open) return cfg;
  cfg.wifiSsid     = _prefs.getString(CamConfig::kNvsWifiSsid, "");
  cfg.wifiPassword = _prefs.getString(CamConfig::kNvsWifiPass, "");

  cfg.mqttHost     = _prefs.getString(CamConfig::kNvsMqttHost, String(CamConfig::kDefaultMqttHost));
  cfg.mqttPort     = _prefs.getUShort(CamConfig::kNvsMqttPort, CamConfig::kDefaultMqttPort);

  // Migrate stale NVS values (e.g. port 8883 saved from a previous firmware build)
  if (cfg.mqttHost == "phasmida.eu" || cfg.mqttPort == 8883) {
    cfg.mqttHost = String(CamConfig::kDefaultMqttHost);
    cfg.mqttPort = CamConfig::kDefaultMqttPort;
    _prefs.putString(CamConfig::kNvsMqttHost, cfg.mqttHost);
    _prefs.putUShort(CamConfig::kNvsMqttPort, cfg.mqttPort);
  }

  return cfg;
}

void CameraConfigStore::setWifi(const String& ssid, const String& pass) {
  if (!_open) return;
  _prefs.putString(CamConfig::kNvsWifiSsid, ssid);
  _prefs.putString(CamConfig::kNvsWifiPass, pass);
}

void CameraConfigStore::setJpegQuality(uint8_t quality) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsJpegQuality, quality);
}

void CameraConfigStore::setFrameSize(uint8_t frameSize) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsFrameSize, frameSize);
}

uint8_t CameraConfigStore::getJpegQuality() const {
  if (!_open) return CamConfig::kDefaultJpegQuality;
  return _prefs.getUChar(CamConfig::kNvsJpegQuality, CamConfig::kDefaultJpegQuality);
}

uint8_t CameraConfigStore::getFrameSize() const {
  if (!_open) return CamConfig::kDefaultFrameSize;
  return _prefs.getUChar(CamConfig::kNvsFrameSize, CamConfig::kDefaultFrameSize);
}

void CameraConfigStore::setSharpness(int8_t sharpness) {
  if (!_open) return;
  _prefs.putChar(CamConfig::kNvsSharpness, sharpness);
}

void CameraConfigStore::setDenoise(uint8_t denoise) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsDenoise, denoise);
}

void CameraConfigStore::setLenc(bool enabled) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsLenc, enabled ? 1 : 0);
}

void CameraConfigStore::setRawGma(bool enabled) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsRawGma, enabled ? 1 : 0);
}

void CameraConfigStore::setAec2(bool enabled) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsAec2, enabled ? 1 : 0);
}

void CameraConfigStore::setWpc(bool enabled) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsWpc, enabled ? 1 : 0);
}

void CameraConfigStore::setBpc(bool enabled) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsBpc, enabled ? 1 : 0);
}

void CameraConfigStore::setGainCeiling(uint8_t gainCeiling) {
  if (!_open) return;
  _prefs.putUChar(CamConfig::kNvsGainCeiling, gainCeiling);
}

int8_t CameraConfigStore::getSharpness() const {
  if (!_open) return CamConfig::kDefaultSharpness;
  return _prefs.getChar(CamConfig::kNvsSharpness, CamConfig::kDefaultSharpness);
}

uint8_t CameraConfigStore::getDenoise() const {
  if (!_open) return CamConfig::kDefaultDenoise;
  return _prefs.getUChar(CamConfig::kNvsDenoise, CamConfig::kDefaultDenoise);
}

bool CameraConfigStore::getLenc() const {
  if (!_open) return CamConfig::kDefaultLenc != 0;
  return _prefs.getUChar(CamConfig::kNvsLenc, CamConfig::kDefaultLenc) != 0;
}

bool CameraConfigStore::getRawGma() const {
  if (!_open) return CamConfig::kDefaultRawGma != 0;
  return _prefs.getUChar(CamConfig::kNvsRawGma, CamConfig::kDefaultRawGma) != 0;
}

bool CameraConfigStore::getAec2() const {
  if (!_open) return CamConfig::kDefaultAec2 != 0;
  return _prefs.getUChar(CamConfig::kNvsAec2, CamConfig::kDefaultAec2) != 0;
}

bool CameraConfigStore::getWpc() const {
  if (!_open) return CamConfig::kDefaultWpc != 0;
  return _prefs.getUChar(CamConfig::kNvsWpc, CamConfig::kDefaultWpc) != 0;
}

bool CameraConfigStore::getBpc() const {
  if (!_open) return CamConfig::kDefaultBpc != 0;
  return _prefs.getUChar(CamConfig::kNvsBpc, CamConfig::kDefaultBpc) != 0;
}

uint8_t CameraConfigStore::getGainCeiling() const {
  if (!_open) return CamConfig::kDefaultGainCeiling;
  return _prefs.getUChar(CamConfig::kNvsGainCeiling, CamConfig::kDefaultGainCeiling);
}

void CameraConfigStore::clear() {
  if (!_open) return;
  _prefs.clear();
}
