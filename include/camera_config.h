#pragma once

#include <stdint.h>
#include "esp_camera.h"

#if __has_include("secrets.local.h")
#include "secrets.local.h"
#endif
#include "secrets.example.h"

namespace CamConfig {

// ── WiFi ─────────────────────────────────────────────────────────────────────
constexpr uint32_t    kWifiConnectTimeoutMs    = 20000;
constexpr uint32_t    kWifiReconnectIntervalMs = 30000;
constexpr const char* kProvisioningApPrefix    = "Phasmida-Cam";
constexpr const char* kProvisioningApPassword  = "phasmida123";

// ── Device identity / backend ─────────────────────────────────────────────────
constexpr const char* kDeviceApiKey = PHASMIDA_CAM_DEVICE_API_KEY;
constexpr const char* kWsBaseUrl    = "wss://api.phasmida.eu";

// ── WebSocket heartbeat / reconnect (from spec-firmware-v1-3-camera-streaming)
constexpr uint32_t kWsPingIntervalMs     = 30000;  // send ping every 30 s
constexpr uint32_t kWsHeartbeatTimeoutMs = 60000;  // reconnect if no pong within 60 s
constexpr uint32_t kWsReconnectBaseMs    = 1000;   // exponential backoff base (1 s)
constexpr uint32_t kWsReconnectMaxMs     = 60000;  // cap at 60 s

// ── MQTT (command channel for camera quality control) ────────────────────────
// Camera MQTT login uses kDeviceApiKey (see timer_camera/main.cpp).
constexpr const char* kDefaultMqttHost   = "api.phasmida.eu";
constexpr uint16_t    kDefaultMqttPort   = 1883;
constexpr uint16_t    kMqttKeepAliveSec  = 60;
constexpr uint8_t     kMqttQos           = 1;
constexpr uint32_t    kMqttReconnectBaseDelayMs    = 1000;
constexpr uint32_t    kMqttAuthFailureInitialDelayMs = 30000;
constexpr uint32_t    kMqttReconnectMaxDelayMs     = 60000;
constexpr uint32_t    kMqttConnectTimeoutMs        = 10000;

// ── NVS ──────────────────────────────────────────────────────────────────────
constexpr const char* kNvsNamespace = "phasmida-cam";
constexpr const char* kNvsWifiSsid  = "wifi_ssid";
constexpr const char* kNvsWifiPass  = "wifi_pass";
constexpr const char* kNvsApiKey    = "api_key";
constexpr const char* kNvsWsBaseUrl = "ws_base_url";
constexpr const char* kNvsJpegQuality = "jpeg_quality";  // Store quality 0–63
constexpr const char* kNvsFrameSize    = "frame_size";   // Store framesize index
constexpr const char* kNvsFrameDelay   = "frame_delay";  // Store inter-frame delay in ms (0..2000)
constexpr const char* kNvsSharpness    = "sharpness";    // Store sensor sharpness (-2..2)
constexpr const char* kNvsDenoise      = "denoise";      // Store sensor denoise (0..8)
constexpr const char* kNvsLenc         = "lenc";         // Lens correction (0/1)
constexpr const char* kNvsRawGma       = "raw_gma";      // Raw gamma (0/1)
constexpr const char* kNvsAec2         = "aec2";         // AEC2 (0/1)
constexpr const char* kNvsWpc          = "wpc";          // White pixel correction (0/1)
constexpr const char* kNvsBpc          = "bpc";          // Black pixel correction (0/1)
constexpr const char* kNvsGainCeiling  = "gain_ceil";    // Gain ceiling enum (0..6)
constexpr const char* kNvsVFlip        = "vflip";        // Vertical image flip (0/1)
constexpr const char* kNvsHMirror      = "hmirror";      // Horizontal image mirror (0/1)
constexpr const char* kNvsMqttHost  = "mqtt_host";       // MQTT broker hostname
constexpr const char* kNvsMqttPort  = "mqtt_port";       // MQTT broker port
constexpr const char* kNvsQualityVer = "quality_ver";   // Migration version for quality defaults

// ── Camera Quality / Resolution ──────────────────────────────────────────────
// JPEG quality: 0–63, where lower value means better quality (larger frames)
// Preferred default: 8 for high detail while staying within a practical range.
constexpr uint8_t kDefaultJpegQuality = 8;
// Safe fallback if preferred settings fail during camera init/warmup.
constexpr uint8_t kSafeJpegQuality = 12;

// Frame size enum values from esp_camera.h:
//   FRAMESIZE_QVGA  = 5   (320×240, smallest)
//   FRAMESIZE_VGA   = 8   (640×480, medium)
//   FRAMESIZE_SVGA  = 9   (800×600, safe fallback)
//   FRAMESIZE_XGA   = 10  (1024×768, preferred default)
//   FRAMESIZE_SXGA  = 12  (1280×1024, very large)
//   FRAMESIZE_UXGA  = 13  (1600×1200, maximum exposed by this firmware)
// We use numeric value directly to avoid circular include issues
constexpr uint8_t kDefaultFrameSize = 10;  // FRAMESIZE_XGA (1024×768)
constexpr uint8_t kSafeFrameSize = 9;      // FRAMESIZE_SVGA (800×600)
constexpr uint16_t kDefaultFrameDelay = 0; // 0 = max FPS

// OV3660 tuning defaults
constexpr int8_t  kDefaultSharpness = 2;                         // -2..2
constexpr uint8_t kDefaultDenoise = 0;                           // 0..8
constexpr uint8_t kDefaultLenc = 1;                              // bool 0/1
constexpr uint8_t kDefaultRawGma = 1;                            // bool 0/1
constexpr uint8_t kDefaultAec2 = 1;                              // bool 0/1
constexpr uint8_t kDefaultWpc = 1;                               // bool 0/1
constexpr uint8_t kDefaultBpc = 1;                               // bool 0/1
constexpr uint8_t kDefaultGainCeiling = static_cast<uint8_t>(GAINCEILING_16X);
constexpr uint8_t kDefaultVFlip = 1;                             // bool 0/1, current upright housing default
constexpr uint8_t kDefaultHMirror = 0;                           // bool 0/1, disable L/R mirror by default

// ── Misc ─────────────────────────────────────────────────────────────────────
constexpr const char* kFwVersion = "1.3.0";

}  // namespace CamConfig
