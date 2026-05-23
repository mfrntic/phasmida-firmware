#pragma once

#include <Arduino.h>
#include "esp_camera.h"

class CameraManager {
public:
  // Set camera quality before init(). Both default to values from camera_config.h
  void setJpegQuality(uint8_t quality) { _jpegQuality = quality; }
  void setFrameSize(uint8_t frameSize) { _frameSize = frameSize; }
  void setSharpness(int8_t sharpness) { _sharpness = sharpness; }
  void setDenoise(uint8_t denoise) { _denoise = denoise; }
  void setLenc(bool enabled) { _lenc = enabled; }
  void setRawGma(bool enabled) { _rawGma = enabled; }
  void setAec2(bool enabled) { _aec2 = enabled; }
  void setWpc(bool enabled) { _wpc = enabled; }
  void setBpc(bool enabled) { _bpc = enabled; }
  void setGainCeiling(uint8_t gainCeiling) { _gainCeiling = gainCeiling; }
  
  // Initialise camera sensor and framebuffer allocator. Returns false on failure.
  bool init();
  bool isReady() const { return _ready; }

  // Capture one JPEG frame. Returns nullptr on failure.
  // Caller MUST call releaseFrame() after use to return the framebuffer to the driver.
  const uint8_t* captureFrame(size_t& outLen);
  void releaseFrame();

private:
  bool        _ready = false;
  camera_fb_t* _fb   = nullptr;
  uint8_t _jpegQuality = 12;   // Lower value = better JPEG quality
  uint8_t _frameSize = 9;      // Default 9 = FRAMESIZE_SVGA (800×600), can be set via setFrameSize()
  int8_t _sharpness = 2;       // -2..2
  uint8_t _denoise = 0;        // 0..8
  bool _lenc = true;
  bool _rawGma = true;
  bool _aec2 = true;
  bool _wpc = true;
  bool _bpc = true;
  uint8_t _gainCeiling = static_cast<uint8_t>(GAINCEILING_16X);
};
