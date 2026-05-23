#pragma once

#include <stdint.h>

#include "secrets.example.h"
#if __has_include("secrets.local.h")
#include "secrets.local.h"
#endif

namespace AppConfig {

constexpr uint32_t kTelemetryIntervalMs = 30000;
constexpr uint32_t kMinTelemetryIntervalMs = 5000;
constexpr uint32_t kMqttReconnectBaseDelayMs = 1000;
constexpr uint32_t kMqttAuthFailureInitialDelayMs = 30000;
constexpr uint32_t kMqttReconnectMaxDelayMs = 60000;
constexpr uint32_t kNtpSyncTimeoutMs = 20000;
constexpr uint32_t kWifiConnectTimeoutMs = 20000;  // mirrors hardcoded 20000 u connectWiFiBlocking()
constexpr uint32_t kWifiReconnectIntervalMs = 30000; // kako često pokušavati WiFi reconnect u loop()
constexpr uint32_t kStatusHeartbeatMs = 5 * 60 * 1000;
constexpr bool kEnableWifiProvisioning = true;
constexpr uint16_t kMqttDefaultPort = 1883;
constexpr uint16_t kMqttKeepAliveSec = 60;
constexpr uint8_t kMqttQos = 1;

constexpr int kApiVersion = 1;

// Compile-time credentials — username is MAC slug (runtime), password is API key.
constexpr const char* kMqttApiKey = PHASMIDA_APP_MQTT_API_KEY;
constexpr const char* kFwVersion = "1.0.0";

// Bootstrap defaults used only when values are missing in NVS.
constexpr const char* kDefaultWifiSsid = PHASMIDA_APP_DEFAULT_WIFI_SSID;
constexpr const char* kDefaultWifiPassword = PHASMIDA_APP_DEFAULT_WIFI_PASSWORD;
constexpr const char* kProvisioningApPrefix  = "Phasmida-Setup";
constexpr const char* kProvisioningApPassword = PHASMIDA_APP_PROVISIONING_AP_PASSWORD;
constexpr const char* kDefaultMqttHost = "api.phasmida.eu";

// NVS namespace and keys.
constexpr const char* kNvsNamespace = "phasmida";
constexpr const char* kNvsWifiSsid = "wifi_ssid";
constexpr const char* kNvsWifiPass = "wifi_pass";
constexpr const char* kNvsMqttHost = "mqtt_host";
constexpr const char* kNvsMqttPort = "mqtt_port";
constexpr const char* kNvsTelemetryInterval = "tlm_int_ms";

// Persisted RGB set-light state (survives reboot)
constexpr const char* kNvsLightColor      = "light_color";  // uint32_t packed RGB
constexpr const char* kNvsLightBrightness = "light_brt";    // uint8_t
constexpr const char* kNvsLightCmdId      = "light_cmd";

// LED hardware — M5GO3 Bottom (built-in WS2812 on M5-Bus pin 8 = CoreS3 GPIO5)
constexpr uint8_t  kM5Go3BottomLedPin   = 5;   // M5-Bus RGB signal → CoreS3 GPIO5
constexpr uint8_t  kM5Go3BottomLedCount = 10;

// SK6812 RGB Unit — PORT.C (G17/G18); data on G17
// Daisy-chain: up to 4 units × 3 LEDs = 12 total (indices 0-11).
// Unit 1 = LED[0..2], Unit 2 = LED[3..5], Unit 3 = LED[6..8], Unit 4 = LED[9..11]
constexpr uint8_t  kRgbUnitLedPin     = 17;          // PORT.C data → CoreS3 G17
constexpr uint8_t  kRgbUnitLedCount   = 12;          // 4 units × 3 LEDs

// Soil Moisture probe — M5Stack Unit Earth on CoreS3 PORT.B
constexpr uint8_t kSoilMoistureAnalogPin   = 8;   // PORT.B AOUT (Analog Output, grey wire)
constexpr uint8_t kSoilMoistureDigitalPin  = 9;   // PORT.B DOUT (Digital Output, threshold via trim-pot)

// DLight probe — M5Stack Unit DLight (BH1750FVI) on I2C
constexpr uint8_t kDLightI2cAddr = 0x23;

// RGB Soft Hotplug Verification
constexpr uint32_t kRgbVerifyCooldownMs          = 3000;   // min gap between sessions
constexpr uint32_t kRgbVerifyDefaultDurationMs   = 5000;   // default durationMs
constexpr uint32_t kRgbVerifyDefaultConfirmWindowMs = 15000; // default confirmWindowMs
constexpr uint32_t kRgbVerifyPatternStepMs       = 500;    // ms per colour step in discovery pattern

// Terrarium light bands for DLight status (lux)
constexpr float kTerrariumLuxLowMax = 200.0f;   // < 200 lx  = LOW  (nocna faza, sjena)
constexpr float kTerrariumLuxMedMax = 2000.0f;  // 200-2000  = MED  (opce osvjetljenje)
                                                 // > 2000 lx = HIGH (basking zona)

}  // namespace AppConfig
