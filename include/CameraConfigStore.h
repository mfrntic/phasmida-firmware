#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct CameraConfig {
  String wifiSsid;
  String wifiPassword;
  String mqttHost;      // MQTT broker host
  uint16_t mqttPort;    // MQTT broker port
};

class CameraConfigStore {
public:
  bool begin();
  bool hasWifiCredentials() const;
  CameraConfig load() const;

  void setWifi(const String& ssid, const String& pass);
  
  // Camera quality management
  void setJpegQuality(uint8_t quality);  // 0–63
  void setFrameSize(uint8_t frameSize);  // FRAMESIZE_* enum value
  void setFrameDelay(uint16_t frameDelayMs); // 0..2000 ms
  uint8_t getJpegQuality() const;
  uint8_t getFrameSize() const;
  uint16_t getFrameDelay() const;

  // Camera sensor tuning management
  void setSharpness(int8_t sharpness);         // -2..2
  void setDenoise(uint8_t denoise);            // 0..8
  void setLenc(bool enabled);
  void setRawGma(bool enabled);
  void setAec2(bool enabled);
  void setWpc(bool enabled);
  void setBpc(bool enabled);
  void setGainCeiling(uint8_t gainCeiling);    // gainceiling_t enum value (0..6)
  void setVFlip(bool enabled);
  void setHMirror(bool enabled);
  int8_t getSharpness() const;
  uint8_t getDenoise() const;
  bool getLenc() const;
  bool getRawGma() const;
  bool getAec2() const;
  bool getWpc() const;
  bool getBpc() const;
  uint8_t getGainCeiling() const;
  bool getVFlip() const;
  bool getHMirror() const;

  void clear();

private:
  mutable Preferences _prefs;
  bool _open = false;
};
