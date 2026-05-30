#include <CameraManager.h>

// ── M5Stack Timer Camera F pin map ────────────────────────────────────────────
// Source: https://github.com/m5stack/TimerCam-arduino
// Verify against your specific hardware revision before flashing.
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  15
#define CAM_PIN_XCLK   27
#define CAM_PIN_SIOD   25
#define CAM_PIN_SIOC   23
#define CAM_PIN_D7     19
#define CAM_PIN_D6     36
#define CAM_PIN_D5     18
#define CAM_PIN_D4     39
#define CAM_PIN_D3      5
#define CAM_PIN_D2     34
#define CAM_PIN_D1     35
#define CAM_PIN_D0     32
#define CAM_PIN_VSYNC  22
#define CAM_PIN_HREF   26
#define CAM_PIN_PCLK   21

bool CameraManager::init() {
  if (_ready) {
    if (_fb) {
      esp_camera_fb_return(_fb);
      _fb = nullptr;
    }
    esp_camera_deinit();
    _ready = false;
    Serial.println("[CAM] Reinitializing camera with updated settings...");
  }

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = CAM_PIN_D0;
  config.pin_d1       = CAM_PIN_D1;
  config.pin_d2       = CAM_PIN_D2;
  config.pin_d3       = CAM_PIN_D3;
  config.pin_d4       = CAM_PIN_D4;
  config.pin_d5       = CAM_PIN_D5;
  config.pin_d6       = CAM_PIN_D6;
  config.pin_d7       = CAM_PIN_D7;
  config.pin_xclk     = CAM_PIN_XCLK;
  config.pin_pclk     = CAM_PIN_PCLK;
  config.pin_vsync    = CAM_PIN_VSYNC;
  config.pin_href     = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = (framesize_t)_frameSize;      // Use configured frame size (cast to enum)
  config.jpeg_quality = _jpegQuality;                  // Use configured quality
  config.fb_count     = 2;              // 2 buffers: DMA runs continuously (most stable)
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_LATEST;  // always return freshest frame

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] esp_camera_init failed: 0x%x (quality=%u, framesize=%u)\n", err, _jpegQuality, _frameSize);
    return false;
  }

  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor) {
    // Base tuning for stable auto controls.
    sensor->set_whitebal(sensor, 1);
    sensor->set_awb_gain(sensor, 1);
    sensor->set_exposure_ctrl(sensor, 1);
    sensor->set_gain_ctrl(sensor, 1);
    sensor->set_brightness(sensor, 0);
    sensor->set_contrast(sensor, 1);
    sensor->set_saturation(sensor, 0);
    sensor->set_special_effect(sensor, 0);
    sensor->set_quality(sensor, _jpegQuality);
    sensor->set_framesize(sensor, static_cast<framesize_t>(_frameSize));
    sensor->set_vflip(sensor, _vflip ? 1 : 0);
    sensor->set_hmirror(sensor, _hmirror ? 1 : 0);

    // Timer Camera F uses OV3660 fisheye optics; this tuning improves edge clarity
    // and is runtime-configurable through persisted NVS values.
    if (sensor->id.PID == OV3660_PID) {
      sensor->set_lenc(sensor, _lenc ? 1 : 0);
      sensor->set_raw_gma(sensor, _rawGma ? 1 : 0);
      sensor->set_wpc(sensor, _wpc ? 1 : 0);
      sensor->set_bpc(sensor, _bpc ? 1 : 0);
      sensor->set_aec2(sensor, _aec2 ? 1 : 0);
      sensor->set_gainceiling(sensor, static_cast<gainceiling_t>(_gainCeiling));
      sensor->set_denoise(sensor, _denoise);
      sensor->set_sharpness(sensor, _sharpness);

      Serial.printf("[CAM] OV3660 tuning applied (sharpness=%d denoise=%u lenc=%u rawGma=%u aec2=%u wpc=%u bpc=%u gainCeiling=%u)\n",
                    _sharpness,
                    _denoise,
                    _lenc ? 1 : 0,
                    _rawGma ? 1 : 0,
                    _aec2 ? 1 : 0,
                    _wpc ? 1 : 0,
                    _bpc ? 1 : 0,
                    _gainCeiling);
    }
    Serial.printf("[CAM] Orientation applied: rotation=%u deg (vflip=%u, hmirror=%u)\n",
                  (_vflip && _hmirror) ? 180 : 0,
                  _vflip ? 1 : 0,
                  _hmirror ? 1 : 0);
  }

  Serial.printf("[CAM] Initialized with quality=%u (0-63, lower=better) and framesize=%u\n", _jpegQuality, _frameSize);
  _ready = true;
  return true;
}

const uint8_t* CameraManager::captureFrame(size_t& outLen) {
  if (!_ready) { outLen = 0; return nullptr; }

  // Return any previously held framebuffer first
  if (_fb) {
    esp_camera_fb_return(_fb);
    _fb = nullptr;
  }

  uint32_t t0 = millis();
  _fb = esp_camera_fb_get();
  uint32_t fbMs = millis() - t0;
  if (fbMs > 50) {
    Serial.printf("[CAM] fb_get blocked %u ms\n", fbMs);
  }
  if (!_fb) {
    Serial.println("[CAM] esp_camera_fb_get() returned null");
    outLen = 0;
    return nullptr;
  }

  outLen = _fb->len;
  return _fb->buf;
}

void CameraManager::releaseFrame() {
  if (_fb) {
    esp_camera_fb_return(_fb);
    _fb = nullptr;
  }
}
