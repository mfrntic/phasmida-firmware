#include <Arduino.h>
#include <WiFi.h>
#include <ConfigStore.h>
#include <M5Unified.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <app_config.h>
#include <DeviceIdentity.h>

#include <net/TimeSync.h>
#include <net/WifiManager.h>
#include <net/MqttClient.h>
#include <stdarg.h>

#include <sensors/SensorReading.h>
#include <sensors/ProbeRegistry.h>
#include <sensors/Env3Probe.h>
#include <sensors/EnvProProbe.h>
#include <sensors/SoilMoistureProbe.h>
#include <sensors/DLightProbe.h>

#include <ui/BootLogScreen.h>
#include <ui/SoilMoistureScreen.h>
#include <ui/DLightScreen.h>
#include <ui/EnvSensorScreen.h>
#include <ui/EnvProScreen.h>
#include <ui/SettingsScreen.h>
#include <ui/ScreenManager.h>
#include <ui/splash_logo.h>
#include <led/LedManager.h>
#include <led/RgbVerificationManager.h>
#include <ui/RgbVerificationScreen.h>
#include <ui/RgbLightScreen.h>

namespace {

DeviceIdentity g_identity;
ConfigStore g_configStore;
RuntimeConfig g_runtimeCfg;
TimeSync g_timeSync;
WifiManager g_wifi;
MqttClient g_mqtt;

uint32_t g_telemetryIntervalMs = AppConfig::kTelemetryIntervalMs;
uint32_t g_nextTelemetryAt = 0;
uint32_t g_nextStatusHeartbeatAt = 0;
uint32_t g_nextNtpSyncAttemptAt = 0;
uint32_t g_nextWifiReconnectAt = 0;

// UI instances
BootLogScreen   g_bootLogScreen;
ScreenManager   g_screenMgr;
SettingsScreen  g_settingsScreen(g_screenMgr, g_bootLogScreen);

// Per-probe screens. Each probe owns its own instance(s) so that, in the
// multi-active model, plugging in two probes shows two independent screens.
EnvSensorScreen g_env3BasicScreen("ENV 3");
EnvSensorScreen g_envProBasicScreen("ENV PRO");
EnvProScreen    g_envProExtrasScreen;
SoilMoistureScreen g_soilMoistureScreen;
DLightScreen g_dlightScreen;

// Probe drivers + registry. To add a new sensor unit (Earth, Light, ...):
//   1. Implement an ISensorProbe subclass under include/sensors + src/sensors.
//   2. Declare a global instance here.
//   3. Call g_probes.addProbe(&yourProbe) below in setup().
Env3Probe       g_env3Probe(&g_env3BasicScreen);
EnvProProbe     g_envProProbe(&g_envProBasicScreen, &g_envProExtrasScreen);
SoilMoistureProbe g_soilMoistureProbe(&g_soilMoistureScreen);
DLightProbe    g_dlightProbe(&g_dlightScreen);
ProbeRegistry   g_probes;

// cmdId deduplication — circular buffer of recently handled command IDs
constexpr size_t kCmdIdCacheSize = 8;
String g_recentCmdIds[kCmdIdCacheSize];
size_t g_cmdIdCacheHead = 0;

LedManager g_ledMgr;
RgbVerificationManager g_rgbVerification;
RgbVerificationScreen  g_rgbVerificationScreen;
RgbLightScreen         g_rgbLightScreen(g_ledMgr, g_configStore);

void logLine(const String& line);
void logf(const char* fmt, ...);
void bootLog(uint32_t& step, const String& message);
bool publishEvent(const char* type, const char* severity, const char* message, const char* detailsJson = nullptr);

static bool isCmdIdSeen(const String& id) {
  for (size_t i = 0; i < kCmdIdCacheSize; ++i) {
    if (g_recentCmdIds[i] == id) return true;
  }
  return false;
}

static void markCmdIdSeen(const String& id) {
  g_recentCmdIds[g_cmdIdCacheHead] = id;
  g_cmdIdCacheHead = (g_cmdIdCacheHead + 1) % kCmdIdCacheSize;
}

// Task 7.3 — logLine routes to BootLogScreen
void logLine(const String& line) {
  Serial.println(line);
  g_bootLogScreen.pushLine(line);
}

void logf(const char* fmt, ...) {
  char buffer[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);
  logLine(String(buffer));
}

void bootLog(uint32_t& step, const String& message) {
  logf("[BOOT %02lu][+%lums] %s",
       static_cast<unsigned long>(step++),
       static_cast<unsigned long>(millis()),
       message.c_str());
}

void logBootStep(uint32_t& step, const String& message) {
  bootLog(step, message);
}

String generateUuidV4() {
  uint32_t a = esp_random();
  uint32_t b = esp_random();
  uint32_t c = esp_random();
  uint32_t d = esp_random();

  uint16_t part3 = static_cast<uint16_t>((b >> 16) & 0x0FFF);
  part3 |= 0x4000;  // Version 4

  uint16_t part4 = static_cast<uint16_t>((c >> 16) & 0x3FFF);
  part4 |= 0x8000;  // RFC 4122 variant

  char uuid[37] = {0};
  snprintf(uuid,
           sizeof(uuid),
           "%08lX-%04lX-%04X-%04X-%04lX%08lX",
           static_cast<unsigned long>(a),
           static_cast<unsigned long>((b >> 16) & 0xFFFF),
           part3,
           part4,
           static_cast<unsigned long>(c & 0xFFFF),
           static_cast<unsigned long>(d));
  return String(uuid);
}

String provisioningApName() {
  String slug = g_identity.macSlug();
  String suffix = slug;
  if (slug.length() > 4) {
    suffix = slug.substring(slug.length() - 4);
  }
  return String(AppConfig::kProvisioningApPrefix) + "-" + suffix;
}

bool isValidPosixTimezone(const String& tz) {
  if (tz.isEmpty()) return false;
  if (tz.length() > 64) return false;
  for (size_t i = 0; i < tz.length(); ++i) {
    char c = tz.charAt(i);
    if (c < 33 || c > 126) return false;
  }
  return true;
}



static void restorePersistedLightIfValid() {
  PersistedLightState ls = g_configStore.loadLightState();
  if (!ls.valid) return;
  g_ledMgr.applySetLight(ls.color, ls.brightness, ls.cmdId);
  logf("light restore: color=#%02X%02X%02X brt=%u",
       ls.color.r, ls.color.g, ls.color.b, ls.brightness);
}

void logSensorReadings(const char* header, const SensorReading& data, const char* source) {
  logLine(String(header) + " [" + (source ? source : "?") + "]");
  if (data.hasTemperature) logf("  temperature: %.2f C", data.temperatureC);
  if (data.hasHumidity)    logf("  humidity:    %.2f %%", data.humidityPct);
  if (data.hasPressure)    logf("  pressure:    %.2f hPa", data.pressurePa / 100.0f);
  if (data.hasGas)         logf("  gas:         %.0f Ohm", data.gasResistanceOhm);
  if (data.hasIaq)         logf("  iaq:         %.2f (acc=%u)", data.iaq, static_cast<unsigned>(data.iaqAccuracy));
  if (data.hasSoilMoisture) logf("  soil:        %.1f %% (raw=%u, dry=%s)",
                                  data.soilMoisturePct,
                                  static_cast<unsigned>(data.soilMoistureRaw),
                                  data.soilMoistureDry ? "yes" : "no");
  if (data.hasLux)         logf("  lux:         %.1f", data.lux);
}

void onProbePresenceChange(ISensorProbe* probe, bool present) {
  if (!probe) return;
  logf("Probe %s %s", probe->name(), present ? "connected" : "disconnected");

  // Build details JSON with probe name for the events topic
  char detailsJson[64];
  snprintf(detailsJson, sizeof(detailsJson), "{\"probe\":\"%s\"}", probe->name());
  publishEvent(
    present ? "probe-connected" : "probe-disconnected",
    "info",
    present ? "Sensor probe connected" : "Sensor probe disconnected",
    detailsJson
  );
}

bool publishTelemetry();

bool publishEvent(const char* type, const char* severity, const char* message, const char* detailsJson) {
  if (!g_mqtt.isConnected()) return false;

  JsonDocument doc;
  doc["apiVersion"] = AppConfig::kApiVersion;
  doc["msgId"]      = generateUuidV4();
  doc["macaddress"] = g_identity.macDisplay();
  doc["ts"]         = g_timeSync.unixEpochMs();
  doc["type"]       = type;
  doc["severity"]   = severity;
  doc["message"]    = message;

  if (detailsJson != nullptr) {
    JsonDocument details;
    deserializeJson(details, detailsJson);
    doc["details"] = details;
  }

  String payload;
  serializeJson(doc, payload);
  bool ok = g_mqtt.publish(g_identity.eventsTopic(), payload, false);
  logf("Event [%s/%s]: %s", type, severity, ok ? "ok" : "fail");
  return ok;
}

bool publishStatus(const char* state, const char* reason = nullptr) {
  if (!g_mqtt.isConnected()) {
    return false;
  }

  JsonDocument doc;
  doc["state"] = state;
  doc["ts"] = g_timeSync.unixEpochMs();

  if (strcmp(state, "online") == 0) {
    doc["fwVersion"] = AppConfig::kFwVersion;
    doc["ip"] = WiFi.localIP().toString();
  } else if (reason != nullptr) {
    doc["reason"] = reason;
  }

  // LED state — always included so cloud can sync on reconnect
  {
    CRGB c = g_ledMgr.activeColor();
    char colorHex[8];
    snprintf(colorHex, sizeof(colorHex), "#%02X%02X%02X", c.r, c.g, c.b);
    JsonObject led = doc["led"].to<JsonObject>();
    led["active"]      = g_ledMgr.isLightActive();
    led["activeColor"] = colorHex;
    led["brightness"]  = g_ledMgr.activeBrightness();
    led["lastCmdId"]   = g_ledMgr.lastSetLightCmdId();
  }

  String payload;
  serializeJson(doc, payload);
  bool ok = g_mqtt.publish(g_identity.statusTopic(), payload, true);
  logf("Status publish (%s): %s", state, ok ? "ok" : "fail");
  return ok;
}

bool publishCommandAck(const String& cmdId, const char* status, const char* errorCode = nullptr, const char* errorMessage = nullptr, const char* resultJson = nullptr) {
  if (!g_mqtt.isConnected()) {
    return false;
  }

  JsonDocument ack;
  ack["cmdId"] = cmdId;
  ack["status"] = status;
  ack["ts"] = g_timeSync.unixEpochMs();

  if (errorCode != nullptr || errorMessage != nullptr) {
    JsonObject err = ack["error"].to<JsonObject>();
    if (errorCode != nullptr) {
      err["code"] = errorCode;
    }
    if (errorMessage != nullptr) {
      err["message"] = errorMessage;
    }
  }

  if (resultJson != nullptr) {
    JsonDocument result;
    deserializeJson(result, resultJson);
    ack["result"] = result;
  }

  String payload;
  serializeJson(ack, payload);
  bool ok = g_mqtt.publish(g_identity.cmdAckTopic(), payload, false);
  logf("CMD ACK publish: %s (cmdId=%s)", ok ? "ok" : "fail", cmdId.c_str());
  return ok;
}

static bool publishRgbVerificationResult(
    const String& sessionId, const char* result, const char* reason,
    uint32_t durationMs, uint32_t confirmWindowMs, const char* pattern) {
  if (!g_mqtt.isConnected()) return false;

  JsonDocument doc;
  doc["msgId"]     = generateUuidV4();
  doc["ts"]        = g_timeSync.unixEpochMs();
  doc["type"]      = "rgb-verification-result";
  doc["sessionId"] = sessionId;
  doc["result"]    = result;
  doc["reason"]    = reason;
  JsonObject meta  = doc["meta"].to<JsonObject>();
  meta["pattern"]         = pattern;
  meta["durationMs"]      = durationMs;
  meta["confirmWindowMs"] = confirmWindowMs;

  String payload;
  serializeJson(doc, payload);
  bool ok = g_mqtt.publish(g_identity.eventsTopic(), payload, false);
  logf("rgb.verify.event.published %s %s", sessionId.c_str(), result);
  return ok;
}

void handleMqttCommand(const String& topicStr, const String& payloadStr) {
  if (topicStr != g_identity.cmdTopic()) {
    logf("MQTT msg ignored on %s", topicStr.c_str());
    return;
  }

  JsonDocument cmd;
  DeserializationError err = deserializeJson(cmd, payloadStr);
  if (err) {
    logf("CMD parse error: %s", err.c_str());
    return;
  }

  String cmdId = cmd["cmdId"] | "";
  String type = cmd["type"] | "";
  uint64_t issuedAt = cmd["issuedAt"] | 0ULL;
  uint32_t ttlMs = cmd["ttlMs"] | 0U;

  if (cmdId.isEmpty() || type.isEmpty() || issuedAt == 0 || ttlMs == 0) {
    logLine("CMD invalid: missing required fields");
    return;
  }

  // Dedup: if we have already executed this cmdId, just re-send ack and skip
  if (isCmdIdSeen(cmdId)) {
    logf("CMD duplicate ignored, re-acking (cmdId=%s)", cmdId.c_str());
    publishCommandAck(cmdId, "ok");
    return;
  }
  markCmdIdSeen(cmdId);

  uint64_t now = g_timeSync.unixEpochMs();
  if (now > (issuedAt + ttlMs)) {
    publishCommandAck(cmdId, "expired");
    return;
  }

  if (type == "request-telemetry") {
    bool ok = publishTelemetry();
    publishCommandAck(cmdId, ok ? "ok" : "error", ok ? nullptr : "telemetry_failed", ok ? nullptr : "Telemetry publish failed");
    return;
  }

  if (type == "reboot") {
    publishCommandAck(cmdId, "ok");
    uint32_t delayMs = cmd["params"]["delayMs"] | 0U;
    delay(delayMs);
    publishStatus("offline", "shutdown");
    delay(200);  // brief window for MQTT to flush
    ESP.restart();
    return;
  }

  if (type == "set-config") {
    uint32_t requestedInterval = cmd["params"]["telemetryIntervalMs"] | g_telemetryIntervalMs;
    g_telemetryIntervalMs = max(requestedInterval, AppConfig::kMinTelemetryIntervalMs);
    g_runtimeCfg.telemetryIntervalMs = g_telemetryIntervalMs;  // keep in sync
    g_configStore.setTelemetryInterval(g_telemetryIntervalMs);
    g_nextTelemetryAt = millis() + g_telemetryIntervalMs;
    char resultJson[48];
    snprintf(resultJson, sizeof(resultJson), "{\"telemetryIntervalMs\":%lu}", (unsigned long)g_telemetryIntervalMs);
    publishCommandAck(cmdId, "ok", nullptr, nullptr, resultJson);
    return;
  }

  if (type == "set-timezone") {
    String posixTz = cmd["params"]["posixTz"] | "";
    if (!isValidPosixTimezone(posixTz)) {
      publishCommandAck(cmdId, "rejected", "invalid_timezone",
                        "params.posixTz must be a non-empty POSIX TZ string (max 64 chars)");
      return;
    }

    if (!g_timeSync.applyTimezone(posixTz)) {
      publishCommandAck(cmdId, "error", "timezone_apply_failed",
                        "Failed to apply timezone on device");
      return;
    }

    g_configStore.setTimezone(posixTz);
    char resultJson[128];
    snprintf(resultJson, sizeof(resultJson),
             "{\"appliedTz\":\"%s\",\"clockMode\":\"local\"}",
             posixTz.c_str());
    publishCommandAck(cmdId, "ok", nullptr, nullptr, resultJson);
    return;
  }

  if (type == "set-led") {
    String mode     = cmd["params"]["mode"]     | "off";
    String colorStr = cmd["params"]["color"]    | "white";
    uint32_t durationMs = cmd["params"]["durationMs"] | 0U;
    g_ledMgr.applyCommand(mode.c_str(), colorStr.c_str(), durationMs);
    publishCommandAck(cmdId, "ok");
    return;
  }

  if (type == "set-light") {
    // Block set-light during active RGB verification session
    if (g_rgbVerification.isPending()) {
      publishCommandAck(cmdId, "rejected", "verification_in_progress",
                        "RGB verification session is active");
      return;
    }

    // Validate targetColor — must be "#RRGGBB"
    const char* colorStr = cmd["params"]["targetColor"] | "";
    CRGB targetColor;
    if (!LedManager::parseHexColor(colorStr, targetColor)) {
      publishCommandAck(cmdId, "rejected", "invalid_color",
                        "targetColor must be a valid #RRGGBB hex string");
      return;
    }

    // Validate brightness [0..255]
    int brightness = cmd["params"]["brightness"] | -1;
    if (brightness < 0 || brightness > 255) {
      publishCommandAck(cmdId, "rejected", "invalid_brightness",
                        "brightness must be 0-255");
      return;
    }

    uint64_t appliedAt = g_timeSync.unixEpochMs();
    g_ledMgr.applySetLight(targetColor, static_cast<uint8_t>(brightness), cmdId);
    g_configStore.saveLightState(targetColor, static_cast<uint8_t>(brightness), cmdId);

    char activeColorHex[8];
    snprintf(activeColorHex, sizeof(activeColorHex), "#%02X%02X%02X",
             targetColor.r, targetColor.g, targetColor.b);
    char resultJson[96];
    snprintf(resultJson, sizeof(resultJson),
             "{\"activeColor\":\"%s\",\"brightness\":%d,\"appliedAt\":%llu}",
             activeColorHex, brightness, (unsigned long long)appliedAt);
    publishCommandAck(cmdId, "ok", nullptr, nullptr, resultJson);
    return;
  }

  if (type == "start-rgb-verification") {
    String sessionId      = cmd["params"]["sessionId"]      | "";
    String pattern        = cmd["params"]["pattern"]        | "";
    uint32_t durationMs      = cmd["params"]["durationMs"]   | AppConfig::kRgbVerifyDefaultDurationMs;
    uint32_t confirmWindowMs = cmd["params"]["confirmWindowMs"] | AppConfig::kRgbVerifyDefaultConfirmWindowMs;

    bool   ackOk = false;
    String ackError;
    g_rgbVerification.handleCommand(cmdId, sessionId, pattern,
                                    durationMs, confirmWindowMs,
                                    ackOk, ackError);
    if (ackOk) {
      logf("rgb.verify.start %s", sessionId.c_str());
      // Show verification screen
      g_rgbVerificationScreen.setSession(sessionId, confirmWindowMs, millis());
      g_screenMgr.showTransient(&g_rgbVerificationScreen);
      logf("rgb.verify.prompt.shown %s", sessionId.c_str());
      // Build ACK result
      char resultJson[192];  // 43 bytes overhead + 128 max sessionId + margin
      snprintf(resultJson, sizeof(resultJson),
               "{\"sessionId\":\"%s\",\"state\":\"pending_verification\"}",
               sessionId.c_str());
      publishCommandAck(cmdId, "ok", nullptr, nullptr, resultJson);
    } else {
      // Specijalni slučaj: led_driver_unavailable
      if (ackError == "led_driver_unavailable") {
        publishCommandAck(cmdId, "rejected", "led_driver_unavailable", "LED driver unavailable");
      } else {
        publishCommandAck(cmdId, "rejected", ackError.c_str(), nullptr);
      }
    }
    return;
  }

  if (type == "factory-reset") {
    bool confirm = cmd["params"]["confirm"] | false;
    if (!confirm) {
      publishCommandAck(cmdId, "rejected", "confirm_required", "factory-reset requires confirm=true");
      return;
    }
    publishCommandAck(cmdId, "ok");
    publishEvent("factory-reset", "info", "Factory reset initiated, rebooting");
    delay(200);
    publishStatus("offline", "factory-reset");
    delay(100);
    g_configStore.clear();
    ESP.restart();
    return;
  }

  publishCommandAck(cmdId, "rejected", "unsupported_command", "Unsupported command type");
}

// Build and publish one telemetry MQTT message for a single probe.
// All probes in the same cycle share the same `tsMs` so readings can be
// correlated on the backend. Each message gets its own `msgId`.
// Returns the number of metrics serialized (0 = nothing to publish).
static size_t publishProbe(ISensorProbe* probe, const SensorReading& r, uint64_t tsMs) {
  JsonDocument doc;
  doc["apiVersion"]  = AppConfig::kApiVersion;
  doc["msgId"]       = generateUuidV4();
  doc["macaddress"]  = g_identity.macDisplay();
  doc["sensorType"]  = probe->telemetryType();
  doc["probe"]       = probe->name();
  doc["timestampMs"] = tsMs;

  JsonArray measurements = doc["measurements"].to<JsonArray>();
  size_t    cnt          = 0;

  auto add = [&](const char* metric, float value, const char* unit) {
    JsonObject m = measurements.add<JsonObject>();
    m["metric"] = metric;
    m["value"]  = value;
    m["unit"]   = unit;
    ++cnt;
  };

  if (r.hasTemperature)  add("temperature",   r.temperatureC,     "C");
  if (r.hasHumidity)     add("humidity",      r.humidityPct,      "percent");
  if (r.hasPressure)     add("pressure",      r.pressurePa,       "Pa");
  if (r.hasGas)          add("gasResistance", r.gasResistanceOhm, "Ohm");
  if (r.hasIaq) {
    JsonObject m  = measurements.add<JsonObject>();
    m["metric"]   = "iaq";
    m["value"]    = r.iaq;
    m["unit"]     = "index";
    m["accuracy"] = r.iaqAccuracy;
    ++cnt;
  }
  if (r.hasSoilMoisture) {
    add("soilMoisture",    r.soilMoisturePct, "percent");
    add("soilMoistureRaw", static_cast<float>(r.soilMoistureRaw), "raw");
    JsonObject m  = measurements.add<JsonObject>();
    m["metric"]   = "soilMoistureDry";
    m["value"]    = r.soilMoistureDry ? 1 : 0;
    m["unit"]     = "bool";
    ++cnt;
  }
  if (r.hasLux)          add("illuminance",  r.lux,             "lx");

  if (cnt == 0) return 0;

  String payload;
  serializeJson(doc, payload);

  bool ok = g_mqtt.publish(g_identity.telemetryTopic(), payload, false);
  logf("Telemetry [%s]: %s (%u metric%s)",
       probe->name(),
       ok ? "ok" : "fail",
       static_cast<unsigned>(cnt),
       cnt == 1 ? "" : "s");
  return ok ? cnt : 0;
}

bool publishTelemetry() {
  uint64_t tsMs = g_timeSync.unixEpochMs();
  if (tsMs == 0) {
    logLine("Telemetry: invalid timestamp");
    return false;
  }

  // Publish one message per present probe. Probes are independent — each
  // message contains only its own probe's metrics so metric names are always
  // unique within a payload, as required by the backend contract.
  size_t publishedProbes = 0;
  for (size_t i = 0; i < g_probes.probeCount(); ++i) {
    if (!g_probes.isPresent(i)) continue;
    SensorReading r{};
    if (!g_probes.lastReading(i, r)) continue;

    ISensorProbe* probe = g_probes.probeAt(i);
    logSensorReadings("Telemetry readings:", r, probe->name());
    if (publishProbe(probe, r, tsMs) > 0) ++publishedProbes;
  }

  if (publishedProbes == 0) {
    logLine("Telemetry: no probe data available");
    return false;
  }

  logf("Telemetry cycle: %u probe message%s published",
       static_cast<unsigned>(publishedProbes),
       publishedProbes == 1 ? "" : "s");
  return true;
}

}  // namespace

void setup() {
    // --- HOOK: Provjera je li bila aktivna RGB verifikacija prije reboota ---
    // TODO: Ovdje bi trebalo učitati zadnju sessionId iz NVRAM/RTC/flash i ako je bila pending, odmah publishRgbVerificationResult(..., "timeout", "confirm_timeout", ...)
    // Za sada nije implementirano jer zahtijeva dodatnu persistentnu pohranu.
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  delay(300);

  // Splash screen — prikazuje logotip ~2.5s prije boot loga
  // Slika je točno 320x240px — fullscreen prikaz bez offseta
  M5.Display.fillScreen(0x0000);
  M5.Display.drawJpg(kSplashLogoJpg, kSplashLogoJpgLen, 0, 0);
  delay(2500);
  M5.Display.fillScreen(0x0000);

  randomSeed(esp_random());
  uint32_t bootStep = 1;

  // Task 7.2 — init ScreenManager and set boot screen before first logLine
  g_bootLogScreen.setScreenManager(&g_screenMgr);
  g_screenMgr.setBootScreen(&g_bootLogScreen);
  g_settingsScreen.setWifiConfiguredCallback([]() {
    return g_configStore.hasWifiCredentials();
  });
  g_settingsScreen.setWifiResetCallback([]() {
    logLine("WiFi reset requested from Settings screen");
    g_configStore.setWifi("", "");
    delay(200);
    ESP.restart();
  });

  logLine("");
  logLine("========== BOOT START ==========");
  logBootStep(bootStep, "Device starting");
  logBootStep(bootStep, "Initializing identity and MQTT topics");

  g_identity.init();

  logBootStep(bootStep, "Identity and MQTT topics initialized");
  logf("Device MAC: %s (slug: %s)", g_identity.macDisplay().c_str(), g_identity.macSlug().c_str());
  logf("MQTT clientId: %s", g_identity.mqttClientId().c_str());

  logBootStep(bootStep, "Opening NVS namespace");
  if (!g_configStore.begin()) {
    logLine("NVS: failed to open preferences namespace");
  } else {
    logBootStep(bootStep, "NVS namespace opened");
  }

  logBootStep(bootStep, "Loading runtime config from NVS");
  g_configStore.loadDefaults();

  if (AppConfig::kEnableWifiProvisioning && !g_configStore.hasWifiCredentials()) {
    logBootStep(bootStep, "No WiFi credentials in NVS; entering provisioning mode");
    String newSsid;
    String newPassword;
    String apName = provisioningApName();
    logf("WiFi provisioning AP started: '%s'", apName.c_str());
    bool provisioned = g_wifi.startProvisioning(apName, AppConfig::kProvisioningApPassword, newSsid, newPassword);
    if (provisioned) {
      g_configStore.setWifi(newSsid, newPassword);
      logf("WiFi provisioning completed: SSID='%s'", newSsid.c_str());
    } else {
      logLine("WiFi provisioning failed or aborted");
    }
  }

  g_runtimeCfg = g_configStore.load();
  g_telemetryIntervalMs = g_runtimeCfg.telemetryIntervalMs;
  String persistedTimezone = g_configStore.loadTimezone();
  if (!persistedTimezone.isEmpty()) {
    if (g_timeSync.applyTimezone(persistedTimezone)) {
      logf("Timezone from NVS applied: %s", persistedTimezone.c_str());
    } else {
      logf("Timezone from NVS invalid, falling back to UTC: %s", persistedTimezone.c_str());
    }
  } else {
    logLine("Timezone not configured; clock mode defaults to UTC");
  }
  logBootStep(bootStep, "Runtime configuration loaded");

  logf("Config loaded: WiFi SSID='%s', MQTT host='%s:%u'",
       g_runtimeCfg.wifiSsid.c_str(),
       g_runtimeCfg.mqttHost.c_str(),
       g_runtimeCfg.mqttPort);
  logf("Topics: telemetry=%s status=%s cmd=%s cmdAck=%s",
       g_identity.telemetryTopic().c_str(),
       g_identity.statusTopic().c_str(),
       g_identity.cmdTopic().c_str(),
       g_identity.cmdAckTopic().c_str());

  // Settings screen is always present and serves as the anchor before which
  // probe screens are inserted as they are detected.
  g_screenMgr.addScreen(&g_settingsScreen);

    logBootStep(bootStep, "Registering sensor probes");
    g_probes.attachUi(&g_screenMgr, &g_settingsScreen);
    g_probes.onPresenceChange(onProbePresenceChange);
    // Probe priorities in ProbeRegistry enforce deterministic UX order:
    // ENV III, ENV PRO, SOIL, LIGHT, then non-probe screens.
    g_probes.addProbe(&g_env3Probe);
    g_probes.addProbe(&g_envProProbe);
    g_probes.addProbe(&g_soilMoistureProbe);
    g_probes.addProbe(&g_dlightProbe);
    // Future: g_probes.addProbe(&g_lightProbe);

  logBootStep(bootStep, "Detecting + initializing probes");
  g_probes.begin();
  for (size_t i = 0; i < g_probes.probeCount(); ++i) {
    auto* p = g_probes.probeAt(i);
    logf("  %s: %s", p->name(), g_probes.isPresent(i) ? "present" : "absent");
  }

    // LED Control screen is always before Settings, after all probe screens.
    g_screenMgr.addScreenByPriority(&g_rgbLightScreen, 90, &g_settingsScreen);

  logBootStep(bootStep, "Starting WiFi connection phase");
  logLine("--- WIFI CONNECT PHASE ---");
  bool wifiOk = false;
  if (g_runtimeCfg.wifiSsid.isEmpty()) {
    logLine("WiFi: no SSID configured, skipping connect");
  } else {
    logLine(String("WiFi: connecting to SSID='") + g_runtimeCfg.wifiSsid + "'");
    wifiOk = g_wifi.connect(g_runtimeCfg.wifiSsid, g_runtimeCfg.wifiPassword, AppConfig::kWifiConnectTimeoutMs);
    if (wifiOk) logLine(String("WiFi: SUCCESS! IP=") + WiFi.localIP().toString());
    else logf("WiFi: FAILED, final status=%d [%s]", WiFi.status(), WifiManager::statusToText(WiFi.status()));
  }
  logf("WiFi connection result: %s", wifiOk ? "SUCCESS" : "FAILED");
  logBootStep(bootStep, String("WiFi phase completed: ") + (wifiOk ? "SUCCESS" : "FAILED"));

  if (wifiOk) {
    logBootStep(bootStep, "Starting NTP sync phase");
    logLine("--- NTP SYNC PHASE ---");
    g_timeSync.begin();
    bool ntpOk = g_timeSync.sync(AppConfig::kNtpSyncTimeoutMs);
    if (ntpOk) logf("NTP: SUCCESS! ts=%llu", g_timeSync.unixEpochMs());
    else logf("NTP: TIMEOUT, ts=%llu", g_timeSync.unixEpochMs());
    logBootStep(bootStep, String("NTP sync phase completed: ") + (g_timeSync.isSynced() ? "SYNCED" : "NOT_SYNCED"));
  } else {
    logBootStep(bootStep, "Skipping NTP sync (WiFi disconnected)");
  }

  g_ledMgr.begin();
  logBootStep(bootStep, "LED manager initialized");

  logBootStep(bootStep, "Configuring MQTT client");
  {
    MqttConfig mc;
    mc.host              = g_runtimeCfg.mqttHost;
    mc.port              = g_runtimeCfg.mqttPort;
    mc.clientId          = g_identity.mqttClientId();
    mc.username          = g_identity.macSlug();
    mc.password          = AppConfig::kCoreDeviceApiKey;
    logf("MQTT auth diag: username='%s' key_len=%u", mc.username.c_str(), static_cast<unsigned>(mc.password.length()));
    mc.statusTopicForLwt = g_identity.statusTopic();
    mc.willPayload       = "{\"state\":\"offline\",\"ts\":0,\"reason\":\"unexpected\"}";
    mc.keepAliveSec      = AppConfig::kMqttKeepAliveSec;
    mc.bufferSize        = 1024;
    g_mqtt.begin(mc);
  }
  g_mqtt.onMessage(handleMqttCommand);
  g_mqtt.onStatusChange([](const char* e, const char* d) {
    logf("MQTT: %s %s", e, d ? d : "");
    if (strcmp(e, "connected") == 0) {
      bool subOk = g_mqtt.subscribe(g_identity.cmdTopic(), AppConfig::kMqttQos);
      logf("MQTT: subscribe %s => %s", g_identity.cmdTopic().c_str(), subOk ? "ok" : "fail");
      restorePersistedLightIfValid();
      publishStatus("online");
      g_nextStatusHeartbeatAt = millis() + AppConfig::kStatusHeartbeatMs;
    }
  });
  logBootStep(bootStep, "MQTT client configured");

  if (wifiOk) {
    logBootStep(bootStep, "Starting MQTT connection phase");
    logLine("--- MQTT CONNECT PHASE ---");
    bool mqttOk = g_mqtt.connect();
    logBootStep(bootStep, String("MQTT phase completed: ") + (mqttOk ? "SUCCESS" : "FAILED"));
  } else {
    logBootStep(bootStep, "Skipping MQTT connect (WiFi disconnected)");
  }

  logBootStep(bootStep, "Scheduling periodic tasks");
  g_nextTelemetryAt = millis() + g_telemetryIntervalMs;
  g_nextStatusHeartbeatAt = millis() + AppConfig::kStatusHeartbeatMs;
  logBootStep(bootStep, "Periodic tasks scheduled");

  // RGB Verification: wire up callbacks, then register with manager
  g_rgbVerificationScreen.setConfirmCallback([]() {
    logf("rgb.verify.user.confirmed %s", g_rgbVerification.activeSessionId().c_str());
    g_rgbVerification.onConfirm();
  });
  g_rgbVerificationScreen.setRejectCallback([]() {
    logf("rgb.verify.user.rejected %s", g_rgbVerification.activeSessionId().c_str());
    g_rgbVerification.onReject();
  });
  g_rgbVerification.begin(g_ledMgr,
    [](const String& sessionId, const char* result, const char* reason,
       uint32_t durationMs, uint32_t confirmWindowMs, const char* pattern) {
      g_screenMgr.dismissTransient();
      publishRgbVerificationResult(sessionId, result, reason,
                                   durationMs, confirmWindowMs, pattern);
    });
  logBootStep(bootStep, "RGB verification manager initialized");

  logLine("========== BOOT COMPLETE ==========");
  logLine("boot sequence completed");
  logBootStep(bootStep, "Boot sequence completed");

  // Trigger an initial sample pass so screens have data before the carousel
  // animates in. The registry's first sample tick will fire shortly after.
  g_probes.forceSampleNow();
  for (size_t i = 0; i < g_probes.probeCount(); ++i) {
    SensorReading r{};
    if (g_probes.lastReading(i, r)) {
      logSensorReadings("Boot sensor readings:", r, g_probes.probeAt(i)->name());
    }
  }

  // Task 7.4 — animated transition from BootLogScreen → first available carousel screen
  g_screenMgr.transitionFromBoot();
}

void loop() {
  M5.update();

  // Task 7.5 — new input handlers
  g_screenMgr.handleTouch();
  g_screenMgr.handleButtons();
  g_screenMgr.update();

  if (!g_wifi.isConnected() && !g_runtimeCfg.wifiSsid.isEmpty() && millis() >= g_nextWifiReconnectAt) {
    logLine("--- LOOP: WiFi disconnected, attempting reconnect ---");
    bool ok = g_wifi.connect(g_runtimeCfg.wifiSsid, g_runtimeCfg.wifiPassword, AppConfig::kWifiConnectTimeoutMs);
    if (ok) logLine(String("WiFi: reconnected! IP=") + WiFi.localIP().toString());
    else logf("WiFi: reconnect failed, status=%d [%s]", WiFi.status(), WifiManager::statusToText(WiFi.status()));
    g_nextWifiReconnectAt = millis() + AppConfig::kWifiReconnectIntervalMs;
  }

  if (!g_timeSync.isSynced() && millis() >= g_nextNtpSyncAttemptAt) {
    logLine("--- LOOP: Clock not synced, attempting NTP sync ---");
    bool ok = g_timeSync.sync(AppConfig::kNtpSyncTimeoutMs);
    logf("NTP re-sync: %s", ok ? "ok" : "failed");
    g_nextNtpSyncAttemptAt = millis() + 60000;
  }

  g_mqtt.loop();

  // Drive all sensor probes: per-loop service() ticks (e.g. BSEC scheduler)
  // plus periodic sampling, hot-plug detection, and screen add/remove.
  g_probes.service();

  if (g_mqtt.isConnected() && millis() >= g_nextStatusHeartbeatAt) {
    publishStatus("online");
    g_nextStatusHeartbeatAt = millis() + AppConfig::kStatusHeartbeatMs;
  }

  if (g_mqtt.isConnected() && millis() >= g_nextTelemetryAt) {
    logLine("--- LOOP: Publishing telemetry ---");
    publishTelemetry();
    g_nextTelemetryAt = millis() + g_telemetryIntervalMs;
  }

  // LED service — blink timing and duration expiry
  g_ledMgr.service();

  // RGB verification — timeout detection
  g_rgbVerification.service(millis());

  delay(10);
}