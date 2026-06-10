#pragma once

#include <stdint.h>

#if __has_include("secrets.local.h")
#include "secrets.local.h"
#endif
#include "secrets.example.h"

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
#if defined(PHASMIDA_TARGET_ATOMS3_LITE)
constexpr const char* kCoreDeviceApiKey = PHASMIDA_ATOMS3_LITE_DEVICE_API_KEY;
constexpr const char* kDeviceType = "atoms3_lite";
#elif defined(PHASMIDA_TARGET_CORE_S3)
constexpr const char* kCoreDeviceApiKey = PHASMIDA_CORE_S3_DEVICE_API_KEY;
constexpr const char* kDeviceType = "core_s3";
#else
constexpr const char* kCoreDeviceApiKey = PHASMIDA_CORE_DEVICE_API_KEY;
constexpr const char* kDeviceType = "core";
#endif
constexpr const char* kFwVersion = "1.0.0";

// Bootstrap defaults used only when values are missing in NVS.
constexpr const char* kDefaultWifiSsid = "";
constexpr const char* kDefaultWifiPassword = "";
constexpr const char* kProvisioningApPrefix  = "Phasmida-Setup";
constexpr const char* kProvisioningApPassword = "phasmida123";
constexpr const char* kDefaultMqttHost = "api.phasmida.eu";

// NVS namespace and keys.
constexpr const char* kNvsNamespace = "phasmida";
constexpr const char* kNvsWifiSsid = "wifi_ssid";
constexpr const char* kNvsWifiPass = "wifi_pass";
constexpr const char* kNvsMqttHost = "mqtt_host";
constexpr const char* kNvsMqttPort = "mqtt_port";
constexpr const char* kNvsTelemetryInterval = "tlm_int_ms";
constexpr const char* kNvsTimezone = "timezone";

// Persisted RGB set-light state (survives reboot)
constexpr const char* kNvsLightColor      = "light_color";  // uint32_t packed RGB
constexpr const char* kNvsLightBrightness = "light_brt";    // uint8_t
constexpr const char* kNvsLightCmdId      = "light_cmd";
constexpr const char* kNvsLightLocalOverride = "light_lovr"; // bool: local OFF has priority over set-light

// LED and probe pins are board specific.
#if defined(PHASMIDA_TARGET_ATOMS3_LITE)
// ATOM S3 Lite target (headless):
// - Built-in RGB indicator LED is used as the "bottom" strip replacement.
// - External RGB chain can be connected on a free GPIO pin.
// - Soil moisture defaults use exposed GPIO header pins.
constexpr uint8_t  kM5Go3BottomLedPin   = 35;
constexpr uint8_t  kM5Go3BottomLedCount = 1;

constexpr uint8_t  kRgbUnitLedPin       = 2;   // PORT.CUSTOM yellow wire → Atom S3 Lite G2
constexpr uint8_t  kRgbUnitLedCount     = 60;

constexpr uint8_t kSoilMoistureAnalogPin   = 8;
constexpr uint8_t kSoilMoistureDigitalPin  = 7;
#else
// CoreS3 + M5GO3 Bottom defaults.
constexpr uint8_t  kM5Go3BottomLedPin   = 5;   // M5-Bus RGB signal → CoreS3 GPIO5
constexpr uint8_t  kM5Go3BottomLedCount = 10;

// External RGB chain on PORT.C (G17/G18); data on G17.
// This value is treated as the maximum addressed chain length so the same
// firmware works with both short RGB Unit chains and longer NeoPixel strips.
// Extra addressed pixels are ignored when fewer LEDs are physically connected.
constexpr uint8_t  kRgbUnitLedPin       = 17;  // PORT.C data → CoreS3 G17
constexpr uint8_t  kRgbUnitLedCount     = 60;  // max addressed external chain length

// Soil Moisture probe — M5Stack Unit Earth on CoreS3 PORT.B
constexpr uint8_t kSoilMoistureAnalogPin   = 8;   // PORT.B AOUT (Analog Output, grey wire)
constexpr uint8_t kSoilMoistureDigitalPin  = 9;   // PORT.B DOUT (Digital Output, threshold via trim-pot)

#endif

// I2C bus pins — board specific.
// CoreS3 PORT.A (Grove): SDA=2, SCL=1.
// AtomS3 Lite Grove port: SDA=38, SCL=39.
#if defined(PHASMIDA_TARGET_ATOMS3_LITE)
constexpr uint8_t kI2cSda  = 38;
constexpr uint8_t kI2cScl  = 39;
#else
constexpr uint8_t kI2cSda  = 2;
constexpr uint8_t kI2cScl  = 1;
#endif

constexpr uint32_t kI2cFreq = 400000U;

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
