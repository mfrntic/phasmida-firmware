#include <Arduino.h>
#include <camera_config.h>
#include <DeviceIdentity.h>
#include <net/WifiManager.h>
#include <net/MqttClient.h>
#include <CameraConfigStore.h>
#include <CameraManager.h>
#include <WsCameraClient.h>
#include <ArduinoJson.h>

// ─── Firmware state machine ───────────────────────────────────────────────────
// Mirrors recommended states from spec-firmware-v1-3-camera-streaming.md
enum class FwState : uint8_t {
  BOOT,
  LOAD_CONFIG,
  WIFI_PROVISIONING,
  WIFI_CONNECTING,
  MQTT_CONNECTING,      // New: connect to MQTT after WiFi
  CAMERA_INIT,
  WS_CONNECTING,
  STREAMING,
  STREAM_PAUSED,
  AUTH_FAILED,         // 4401 received — wait for credential update
  CAMERA_INIT_FAILED,
};

static DeviceIdentity    g_identity;
static CameraConfigStore g_configStore;
static CameraConfig      g_config;
static WifiManager       g_wifi;
static CameraManager     g_camera;
static WsCameraClient    g_wsClient;
static MqttClient        g_mqtt;
static FwState           g_state = FwState::BOOT;
static bool              g_streamEnabled = true;
static uint8_t           g_consecutiveDnsFailures = 0;
static bool              g_forceWifiReconnectPending = false;

constexpr uint8_t kDnsFailureWifiReconnectThreshold = 3;

// ─── Command deduplication ────────────────────────────────────────────────────
constexpr size_t kCmdIdCacheSize = 8;
static String g_recentCmdIds[kCmdIdCacheSize];
static size_t g_cmdIdCacheHead = 0;

static bool _isCmdIdSeen(const String& id) {
  for (size_t i = 0; i < kCmdIdCacheSize; ++i) {
    if (g_recentCmdIds[i] == id) return true;
  }
  return false;
}

static void _markCmdIdSeen(const String& id) {
  g_recentCmdIds[g_cmdIdCacheHead] = id;
  g_cmdIdCacheHead = (g_cmdIdCacheHead + 1) % kCmdIdCacheSize;
}

// ─── Helper ──────────────────────────────────────────────────────────────────

static bool publishCommandAck(const String& cmdId, const char* status,
                               const char* errorCode = nullptr,
                               const char* resultJson = nullptr) {
  if (!g_mqtt.isConnected()) return false;

  JsonDocument ack;
  ack["cmdId"] = cmdId;
  ack["status"] = status;
  ack["ts"] = millis();  // Use millis() since timer_camera doesn't have TimeSync

  if (errorCode != nullptr) {
    ack["error"]["code"] = errorCode;
  }

  if (resultJson != nullptr) {
    JsonDocument result;
    deserializeJson(result, resultJson);
    ack["result"] = result;
  }

  String payload;
  serializeJson(ack, payload);
  bool ok = g_mqtt.publish(g_identity.cmdAckTopic(), payload, false);
  Serial.printf("[MQTT] CMD ACK: %s (cmdId=%s)\n", ok ? "ok" : "fail", cmdId.c_str());
  return ok;
}

static void handleMqttCommand(const String& topicStr, const String& payloadStr) {
  if (topicStr != g_identity.cmdTopic()) {
    Serial.printf("[MQTT] Ignoring msg on topic: %s\n", topicStr.c_str());
    return;
  }

  JsonDocument cmd;
  DeserializationError err = deserializeJson(cmd, payloadStr);
  if (err) {
    Serial.printf("[MQTT] CMD parse error: %s\n", err.c_str());
    return;
  }

  String cmdId = cmd["cmdId"] | "";
  String type  = cmd["type"]  | "";

  if (cmdId.isEmpty() || type.isEmpty()) {
    Serial.println("[MQTT] CMD invalid: missing cmdId or type");
    return;
  }

  // Dedup: if we've already seen this cmdId, re-ACK and skip
  if (_isCmdIdSeen(cmdId)) {
    Serial.printf("[MQTT] CMD dedup: %s (already processed)\n", cmdId.c_str());
    // You could re-publish the ACK here if needed
    return;
  }

  if (type == "set-camera-quality") {
    // Parse params
    int jpegQuality = cmd["params"]["jpegQuality"] | -1;
    int frameSize   = cmd["params"]["frameSize"]   | -1;
    int sharpness   = cmd["params"]["sharpness"]   | static_cast<int>(g_configStore.getSharpness());
    int denoise     = cmd["params"]["denoise"]     | static_cast<int>(g_configStore.getDenoise());
    int lenc        = cmd["params"]["lenc"]        | static_cast<int>(g_configStore.getLenc() ? 1 : 0);
    int rawGma      = cmd["params"]["rawGma"]      | static_cast<int>(g_configStore.getRawGma() ? 1 : 0);
    int aec2        = cmd["params"]["aec2"]        | static_cast<int>(g_configStore.getAec2() ? 1 : 0);
    int wpc         = cmd["params"]["wpc"]         | static_cast<int>(g_configStore.getWpc() ? 1 : 0);
    int bpc         = cmd["params"]["bpc"]         | static_cast<int>(g_configStore.getBpc() ? 1 : 0);
    int gainCeiling = cmd["params"]["gainCeiling"] | static_cast<int>(g_configStore.getGainCeiling());

    // Validate jpegQuality [0..63]
    if (jpegQuality < 0 || jpegQuality > 63) {
      publishCommandAck(cmdId, "rejected", "invalid_jpeg_quality");
      _markCmdIdSeen(cmdId);
      return;
    }

    // Validate frameSize against esp_camera framesize_t values.
    const uint8_t validFrameSizes[] = {
      static_cast<uint8_t>(FRAMESIZE_QVGA),
      static_cast<uint8_t>(FRAMESIZE_VGA),
      static_cast<uint8_t>(FRAMESIZE_SVGA),
      static_cast<uint8_t>(FRAMESIZE_XGA)
    };
    bool validSize = false;
    for (uint8_t sz : validFrameSizes) {
      if (frameSize == sz) {
        validSize = true;
        break;
      }
    }
    if (!validSize) {
      publishCommandAck(cmdId, "rejected", "invalid_frame_size");
      _markCmdIdSeen(cmdId);
      return;
    }

    if (sharpness < -2 || sharpness > 2) {
      publishCommandAck(cmdId, "rejected", "invalid_sharpness");
      _markCmdIdSeen(cmdId);
      return;
    }

    if (denoise < 0 || denoise > 8) {
      publishCommandAck(cmdId, "rejected", "invalid_denoise");
      _markCmdIdSeen(cmdId);
      return;
    }

    if ((lenc != 0 && lenc != 1) ||
        (rawGma != 0 && rawGma != 1) ||
        (aec2 != 0 && aec2 != 1) ||
        (wpc != 0 && wpc != 1) ||
        (bpc != 0 && bpc != 1)) {
      publishCommandAck(cmdId, "rejected", "invalid_boolean_tuning_flag");
      _markCmdIdSeen(cmdId);
      return;
    }

    if (gainCeiling < 0 || gainCeiling > 6) {
      publishCommandAck(cmdId, "rejected", "invalid_gain_ceiling");
      _markCmdIdSeen(cmdId);
      return;
    }

    // Save to NVS
    g_configStore.setJpegQuality(static_cast<uint8_t>(jpegQuality));
    g_configStore.setFrameSize(static_cast<uint8_t>(frameSize));
    g_configStore.setSharpness(static_cast<int8_t>(sharpness));
    g_configStore.setDenoise(static_cast<uint8_t>(denoise));
    g_configStore.setLenc(lenc != 0);
    g_configStore.setRawGma(rawGma != 0);
    g_configStore.setAec2(aec2 != 0);
    g_configStore.setWpc(wpc != 0);
    g_configStore.setBpc(bpc != 0);
    g_configStore.setGainCeiling(static_cast<uint8_t>(gainCeiling));

    // Build result
    char resultJson[256];
    snprintf(resultJson, sizeof(resultJson),
         "{\"jpegQuality\":%d,\"frameSize\":%d,\"sharpness\":%d,\"denoise\":%d,\"lenc\":%d,\"rawGma\":%d,\"aec2\":%d,\"wpc\":%d,\"bpc\":%d,\"gainCeiling\":%d,\"appliedAt\":%lu}",
         jpegQuality,
         frameSize,
         sharpness,
         denoise,
         lenc,
         rawGma,
         aec2,
         wpc,
         bpc,
         gainCeiling,
         millis());

    // ACK success
    publishCommandAck(cmdId, "ok", nullptr, resultJson);

    // Trigger camera reinit only while streaming is enabled.
    Serial.printf("[CAM] Quality/tuning change requested: quality=%d frameSize=%d sharpness=%d denoise=%d lenc=%d rawGma=%d aec2=%d wpc=%d bpc=%d gainCeiling=%d\n",
            jpegQuality,
            frameSize,
            sharpness,
            denoise,
            lenc,
            rawGma,
            aec2,
            wpc,
            bpc,
            gainCeiling);
    if (g_streamEnabled) {
      Serial.println("[CAM] Disconnecting WebSocket to reinitialize camera...");
      g_wsClient.disconnect();  // Close the WebSocket connection
      g_state = FwState::CAMERA_INIT;  // Trigger camera reinitialization
    } else {
      Serial.println("[CAM] Stream is paused — quality update will apply on next stream-start");
    }
    _markCmdIdSeen(cmdId);
    return;
  }

  if (type == "stream-stop") {
    if (g_state == FwState::AUTH_FAILED) {
      publishCommandAck(cmdId, "rejected", "auth_failed");
      _markCmdIdSeen(cmdId);
      return;
    }

    if (g_streamEnabled) {
      g_streamEnabled = false;
      g_wsClient.disconnect();
      g_state = FwState::STREAM_PAUSED;
    }

    char resultJson[96];
    snprintf(resultJson, sizeof(resultJson),
             "{\"streamEnabled\":false,\"appliedAt\":%lu}",
             millis());
    publishCommandAck(cmdId, "ok", nullptr, resultJson);
    _markCmdIdSeen(cmdId);
    return;
  }

  if (type == "stream-start") {
    if (g_state == FwState::AUTH_FAILED) {
      publishCommandAck(cmdId, "rejected", "auth_failed");
      _markCmdIdSeen(cmdId);
      return;
    }

    if (!g_streamEnabled) {
      g_streamEnabled = true;
      if (g_wifi.isConnected()) {
        g_state = FwState::CAMERA_INIT;
      } else {
        g_state = FwState::WIFI_CONNECTING;
      }
    }

    char resultJson[95];
    snprintf(resultJson, sizeof(resultJson),
             "{\"streamEnabled\":true,\"appliedAt\":%lu}",
             millis());
    publishCommandAck(cmdId, "ok", nullptr, resultJson);
    _markCmdIdSeen(cmdId);
    return;
  }

  // Unknown command type
  publishCommandAck(cmdId, "rejected", "unsupported_command");
  _markCmdIdSeen(cmdId);
}

static void parseWsUrl(const String& url,
                       String& host, uint16_t& port, bool& useTls) {
  useTls = url.startsWith("wss://");
  port   = useTls ? 443 : 80;
  host   = url;
  host.replace("wss://", "");
  host.replace("ws://",  "");
  int slash = host.indexOf('/');
  if (slash >= 0) host = host.substring(0, slash);
}

// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.printf("\n[BOOT] Phasmida Camera Firmware %s\n", CamConfig::kFwVersion);
  g_state = FwState::LOAD_CONFIG;
}

void loop() {
  switch (g_state) {

    // ── LOAD_CONFIG ──────────────────────────────────────────────────────────
    case FwState::LOAD_CONFIG: {
      if (!g_configStore.begin()) {
        Serial.println("[CONFIG] NVS open failed — using defaults");
      }
      g_config = g_configStore.load();
      if (!g_configStore.hasWifiCredentials()) {
        Serial.println("[CONFIG] No WiFi credentials — entering provisioning");
        g_state = FwState::WIFI_PROVISIONING;
      } else {
        g_state = FwState::WIFI_CONNECTING;
      }
      break;
    }

    // ── WIFI_PROVISIONING ────────────────────────────────────────────────────
    case FwState::WIFI_PROVISIONING: {
      String ssid, pass;
      bool ok = g_wifi.startProvisioning(
          String(CamConfig::kProvisioningApPrefix),
          String(CamConfig::kProvisioningApPassword),
          ssid, pass);
      if (ok && !ssid.isEmpty()) {
        g_configStore.setWifi(ssid, pass);
        g_config = g_configStore.load();
        Serial.printf("[WIFI] Provisioned SSID: %s\n", ssid.c_str());
        g_state = FwState::WIFI_CONNECTING;
      } else {
        Serial.println("[WIFI] Provisioning failed — retrying in 5 s");
        delay(5000);
      }
      break;
    }

    // ── WIFI_CONNECTING ──────────────────────────────────────────────────────
    case FwState::WIFI_CONNECTING: {
      Serial.printf("[WIFI] Connecting to '%s'...\n", g_config.wifiSsid.c_str());
      bool forceReconnect = g_forceWifiReconnectPending;
      g_forceWifiReconnectPending = false;
      if (g_wifi.connect(g_config.wifiSsid, g_config.wifiPassword,
                         CamConfig::kWifiConnectTimeoutMs,
                         forceReconnect)) {
        g_identity.init();
        g_consecutiveDnsFailures = 0;
        Serial.printf("[WIFI] Connected. MAC: %s  Slug: %s\n",
                      g_identity.macDisplay().c_str(),
                      g_identity.macSlug().c_str());
        g_state = FwState::MQTT_CONNECTING;
      } else {
        Serial.println("[WIFI] Connect failed — retrying in 5 s");
        delay(5000);
      }
      break;
    }

    // ── MQTT_CONNECTING ──────────────────────────────────────────────────────
    case FwState::MQTT_CONNECTING: {
      Serial.println("[MQTT] Configuring MQTT client...");
      MqttConfig mc;
      mc.host              = g_config.mqttHost.c_str();
      mc.port              = g_config.mqttPort;
      mc.clientId          = g_identity.mqttClientId();
      mc.username          = g_identity.macSlug();
      mc.password          = CamConfig::kDeviceApiKey;
      mc.statusTopicForLwt = g_identity.statusTopic();
      mc.willPayload       = "{\"state\":\"offline\",\"reason\":\"unexpected\"}";
      mc.keepAliveSec      = CamConfig::kMqttKeepAliveSec;
      mc.bufferSize        = 512;
      Serial.printf("[MQTT] broker=%s:%d  clientId=%s  username=%s\n",
                    mc.host.c_str(), mc.port,
                    mc.clientId.c_str(), mc.username.c_str());
      g_mqtt.begin(mc);
      g_mqtt.onMessage(handleMqttCommand);
      g_mqtt.onStatusChange([](const char* event, const char* detail) {
        Serial.printf("[MQTT] Status: %s %s\n", event, detail ? detail : "");
        if (strcmp(event, "dns_failed") == 0) {
          g_consecutiveDnsFailures++;
          Serial.printf("[MQTT][DNS] consecutive_failures=%u threshold=%u\n",
                        g_consecutiveDnsFailures,
                        kDnsFailureWifiReconnectThreshold);
          if (g_consecutiveDnsFailures >= kDnsFailureWifiReconnectThreshold &&
              !g_forceWifiReconnectPending) {
            g_forceWifiReconnectPending = true;
            Serial.println("[WIFI] DNS failures crossed threshold — scheduling forced WiFi reconnect");
          }
        } else if (strcmp(event, "connected") == 0) {
          g_consecutiveDnsFailures = 0;
          bool subOk = g_mqtt.subscribe(g_identity.cmdTopic(), CamConfig::kMqttQos);
          Serial.printf("[MQTT] Subscribe %s: %s\n", g_identity.cmdTopic().c_str(), subOk ? "ok" : "fail");
        } else if (strcmp(event, "tcp_failed") == 0 || strcmp(event, "connect_failed") == 0) {
          g_consecutiveDnsFailures = 0;
        }
      });

      // MQTT je neblokirajući — kamera uvijek nastavlja prema streamu.
      // g_mqtt.loop() u STREAMING stanju nastavlja pokušaje u pozadini.
      Serial.println("[MQTT] Connecting (non-blocking)...");
      g_mqtt.connect();  // inicijalni pokušaj; reconnect automatski kroz loop()
      g_state = g_streamEnabled ? FwState::CAMERA_INIT : FwState::STREAM_PAUSED;
      break;
    }

    // ── CAMERA_INIT ──────────────────────────────────────────────────────────
    case FwState::CAMERA_INIT: {
      // Load camera quality and tuning settings from NVS
      uint8_t jpegQuality = g_configStore.getJpegQuality();
      uint8_t frameSize = g_configStore.getFrameSize();
      int8_t sharpness = g_configStore.getSharpness();
      uint8_t denoise = g_configStore.getDenoise();
      bool lenc = g_configStore.getLenc();
      bool rawGma = g_configStore.getRawGma();
      bool aec2 = g_configStore.getAec2();
      bool wpc = g_configStore.getWpc();
      bool bpc = g_configStore.getBpc();
      uint8_t gainCeiling = g_configStore.getGainCeiling();
        bool nonDefaultTuning =
          (sharpness != CamConfig::kDefaultSharpness) ||
          (denoise != CamConfig::kDefaultDenoise) ||
          (lenc != (CamConfig::kDefaultLenc != 0)) ||
          (rawGma != (CamConfig::kDefaultRawGma != 0)) ||
          (aec2 != (CamConfig::kDefaultAec2 != 0)) ||
          (wpc != (CamConfig::kDefaultWpc != 0)) ||
          (bpc != (CamConfig::kDefaultBpc != 0)) ||
          (gainCeiling != CamConfig::kDefaultGainCeiling);
      auto initCameraWithWarmup = [&](uint8_t quality,
                                      uint8_t size,
                                      int8_t requestedSharpness,
                                      uint8_t requestedDenoise,
                                      bool requestedLenc,
                                      bool requestedRawGma,
                                      bool requestedAec2,
                                      bool requestedWpc,
                                      bool requestedBpc,
                                      uint8_t requestedGainCeiling) {
        g_camera.setJpegQuality(quality);
        g_camera.setFrameSize(size);
        g_camera.setSharpness(requestedSharpness);
        g_camera.setDenoise(requestedDenoise);
        g_camera.setLenc(requestedLenc);
        g_camera.setRawGma(requestedRawGma);
        g_camera.setAec2(requestedAec2);
        g_camera.setWpc(requestedWpc);
        g_camera.setBpc(requestedBpc);
        g_camera.setGainCeiling(requestedGainCeiling);

        if (!g_camera.init()) {
          return false;
        }

        // Warm up the OV3660 sensor: AE/AWB needs several frames to converge.
        // Discard early frames here so captureFrame() is fast once we are
        // in the STREAMING state (avoids 60 s heartbeat timeout on first call).
        Serial.println("[CAM] Sensor warming up (discarding first 10 frames)...");
        int capturedFrames = 0;
        for (int i = 0; i < 10; i++) {
          size_t dummy = 0;
          const uint8_t* frame = g_camera.captureFrame(dummy);
          if (frame != nullptr && dummy > 0) {
            capturedFrames++;
          }
          g_camera.releaseFrame();
        }

        if (capturedFrames == 0) {
          Serial.printf("[CAM] Warmup failed for quality=%u frameSize=%u (no frames captured)\n",
                        quality,
                        size);
          return false;
        }

        Serial.printf("[CAM] Sensor ready (%d/10 warmup frames captured)\n", capturedFrames);
        return true;
      };

      if (initCameraWithWarmup(jpegQuality,
                               frameSize,
                               sharpness,
                               denoise,
                               lenc,
                               rawGma,
                               aec2,
                               wpc,
                               bpc,
                               gainCeiling)) {
        g_state = FwState::WS_CONNECTING;
      } else if (jpegQuality != CamConfig::kDefaultJpegQuality ||
                 frameSize != CamConfig::kDefaultFrameSize ||
                 nonDefaultTuning) {
        Serial.printf("[CAM] Falling back to safe defaults quality=%u frameSize=%u\n",
                      CamConfig::kDefaultJpegQuality,
                      CamConfig::kDefaultFrameSize);
        g_configStore.setJpegQuality(CamConfig::kDefaultJpegQuality);
        g_configStore.setFrameSize(CamConfig::kDefaultFrameSize);
        g_configStore.setSharpness(CamConfig::kDefaultSharpness);
        g_configStore.setDenoise(CamConfig::kDefaultDenoise);
        g_configStore.setLenc(CamConfig::kDefaultLenc != 0);
        g_configStore.setRawGma(CamConfig::kDefaultRawGma != 0);
        g_configStore.setAec2(CamConfig::kDefaultAec2 != 0);
        g_configStore.setWpc(CamConfig::kDefaultWpc != 0);
        g_configStore.setBpc(CamConfig::kDefaultBpc != 0);
        g_configStore.setGainCeiling(CamConfig::kDefaultGainCeiling);

        if (initCameraWithWarmup(CamConfig::kDefaultJpegQuality,
                                 CamConfig::kDefaultFrameSize,
                                 CamConfig::kDefaultSharpness,
                                 CamConfig::kDefaultDenoise,
                                 CamConfig::kDefaultLenc != 0,
                                 CamConfig::kDefaultRawGma != 0,
                                 CamConfig::kDefaultAec2 != 0,
                                 CamConfig::kDefaultWpc != 0,
                                 CamConfig::kDefaultBpc != 0,
                                 CamConfig::kDefaultGainCeiling)) {
          g_state = FwState::WS_CONNECTING;
        } else {
          Serial.println("[CAM] Init failed even after fallback to safe defaults");
          g_state = FwState::CAMERA_INIT_FAILED;
        }
      } else {
        Serial.println("[CAM] Init failed");
        g_state = FwState::CAMERA_INIT_FAILED;
      }
      break;
    }

    // ── WS_CONNECTING ────────────────────────────────────────────────────────
    case FwState::WS_CONNECTING: {
      if (!g_streamEnabled) {
        g_state = FwState::STREAM_PAUSED;
        break;
      }

      String host;
      uint16_t port;
      bool useTls;
      parseWsUrl(CamConfig::kWsBaseUrl, host, port, useTls);
      g_wsClient.begin(host, port, g_identity.macSlug(), CamConfig::kDeviceApiKey, useTls);
      g_state = FwState::STREAMING;
      break;
    }

    // ── STREAMING ────────────────────────────────────────────────────────────
    case FwState::STREAMING: {
      if (!g_streamEnabled) {
        g_wsClient.disconnect();
        g_state = FwState::STREAM_PAUSED;
        break;
      }

      if (g_forceWifiReconnectPending) {
        Serial.println("[WIFI] Executing forced reconnect after repeated DNS failures");
        g_wsClient.disconnect();
        g_state = FwState::WIFI_CONNECTING;
        break;
      }

      if (!g_wifi.isConnected()) {
        Serial.println("[WIFI] Connection lost — reconnecting");
        g_state = FwState::WIFI_CONNECTING;
        break;
      }

      // Process MQTT commands (set-camera-quality, etc.)
      g_mqtt.loop();
      if (g_state != FwState::STREAMING) {
        break;
      }

      g_wsClient.loop();

      if (g_wsClient.state() == WsState::AUTH_FAILED) {
        g_state = FwState::AUTH_FAILED;
        break;
      }

      // Always cycle camera buffers — keeps DMA active during backoff/reconnect.
      // Without this, the single WHEN_EMPTY buffer stays full between reconnects
      // and esp_camera_fb_get() blocks indefinitely on the next streaming cycle.
      g_wsClient.touchHeartbeat();  // safety: reset timer before any blocking DMA wait

      size_t frameLen = 0;
      const uint8_t* frame = g_camera.captureFrame(frameLen);

      if (g_wsClient.isStreaming() && frame && frameLen >= 2) {
        if (frame[0] == 0xFF && frame[1] == 0xD8) {
          bool sent = g_wsClient.sendFrame(frame, frameLen);
          Serial.printf("[CAM] frame %u B \u2192 %s\n", frameLen, sent ? "sent" : "fail");
        } else {
          Serial.printf("[CAM] bad JPEG SOI %02X%02X (%u B) — skip\n",
                        frame[0], frame[1], frameLen);
        }
      }

      g_camera.releaseFrame();
      g_wsClient.loop();  // flush any server close/error received during capture
      break;
    }

    // ── STREAM_PAUSED ───────────────────────────────────────────────────────
    case FwState::STREAM_PAUSED: {
      if (g_forceWifiReconnectPending) {
        Serial.println("[WIFI] Executing forced reconnect after repeated DNS failures");
        g_state = FwState::WIFI_CONNECTING;
        break;
      }

      if (!g_wifi.isConnected()) {
        Serial.println("[WIFI] Connection lost while stream paused — reconnecting");
        g_state = FwState::WIFI_CONNECTING;
        break;
      }

      // Keep command channel alive so cloud can resume streaming.
      g_mqtt.loop();
      break;
    }

    // ── AUTH_FAILED ──────────────────────────────────────────────────────────
    case FwState::AUTH_FAILED: {
      // 4401 — credential problem, don't retry aggressively.
      // Fix: update compile-time CamConfig::kDeviceApiKey and rebuild.
      Serial.println("[WS] AUTH_FAILED (4401) — check compile-time deviceApiKey. Waiting 60 s...");
      delay(60000);
      break;
    }

    // ── CAMERA_INIT_FAILED ───────────────────────────────────────────────────
    case FwState::CAMERA_INIT_FAILED: {
      Serial.println("[CAM] CAMERA_INIT_FAILED — restarting in 10 s");
      delay(10000);
      ESP.restart();
      break;
    }

    case FwState::BOOT:
    default:
      break;
  }
}
