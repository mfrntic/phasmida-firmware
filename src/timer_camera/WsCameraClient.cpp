#include <WsCameraClient.h>
#include <camera_config.h>

WsCameraClient* WsCameraClient::_instance = nullptr;

namespace {
constexpr uint32_t kFastReconnectBaseMs = 500;
constexpr uint32_t kFastReconnectMaxMs = 5000;
constexpr uint32_t kFastReconnectStepMs = 500;
}  // namespace

void WsCameraClient::_staticEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (_instance) _instance->_onEvent(type, payload, length);
}

// ─────────────────────────────────────────────────────────────────────────────

void WsCameraClient::begin(const String& host, uint16_t port,
                            const String& slug, const String& apiKey,
                            bool useTls) {
  _host             = host;
  _port             = port;
  _slug             = slug;
  _apiKey           = apiKey;
  _useTls           = useTls;
  _reconnectDelayMs = 0;
  _connectFailureStreak = 0;
  _instance         = this;
  _connect();
}

void WsCameraClient::_connect() {
  _connectAttemptCount++;
  _connectStartedAt = millis();
  _state = WsState::CONNECTING;
  String path = String("/ws/camera/") + _slug + "?apiKey=" + _apiKey;
  Serial.printf("[WS] Connecting (attempt=%u) → %s:%u%s\n",
                _connectAttemptCount, _host.c_str(), _port, path.c_str());

  if (_useTls) {
    _ws.beginSSL(_host, _port, path);
  } else {
    _ws.begin(_host, _port, path);
  }
  _ws.onEvent(_staticEvent);
  _ws.setReconnectInterval(0);  // reconnect managed here, not by the library
}

// ─────────────────────────────────────────────────────────────────────────────

void WsCameraClient::loop() {
  // AUTH_FAILED is a terminal state — never reconnect automatically
  if (_state == WsState::AUTH_FAILED) return;

  uint32_t now = millis();

  // Waiting for backoff to expire
  if (_state == WsState::BACKOFF || _state == WsState::DISCONNECTED) {
    if (now >= _nextReconnectAt) {
      _connect();
    }
    return;
  }

  _ws.loop();
  now = millis();  // refresh after event processing; PONG may update _lastHeartbeatAt

  if (_state == WsState::STREAMING) {
    // Heartbeat ping
    if (now >= _nextPingAt) {
      _ws.sendPing();
      _lastPingAt = now;
      _sessionPingCount++;
      _nextPingAt = now + CamConfig::kWsPingIntervalMs;
      Serial.printf("[WS] PING tx #%u (session=%u)\n", _sessionPingCount, _sessionId);
    }

    // No pong/activity for 60 s → controlled reconnect
    // _scheduleReconnect() NOT called here — WStype_DISCONNECTED will call it
    if (_lastHeartbeatAt > 0 &&
        (now - _lastHeartbeatAt) > CamConfig::kWsHeartbeatTimeoutMs) {
      Serial.printf("[WS] Heartbeat timeout — reconnecting (idle_ms=%lu, timeout_ms=%lu, session=%u)\n",
                    now - _lastHeartbeatAt, CamConfig::kWsHeartbeatTimeoutMs, _sessionId);
      _ws.disconnect();
    }
  }
}

void WsCameraClient::touchHeartbeat() {
  if (_state == WsState::STREAMING) {
    _lastHeartbeatAt = millis();
  }
}

bool WsCameraClient::sendFrame(const uint8_t* data, size_t len) {
  if (_state != WsState::STREAMING) return false;
  _lastHeartbeatAt = millis();  // reset on every attempt, not just success
  bool ok = _ws.sendBIN(data, len);
  if (ok) {
    _lastFrameSentAt = millis();
    _sessionFrameOkCount++;
  } else {
    _lastFrameFailAt = millis();
    _sessionFrameFailCount++;
    Serial.printf("[WS] sendBIN failed (frame=%u B, session=%u, tx_ok=%u, tx_fail=%u)\n",
                  len, _sessionId, _sessionFrameOkCount, _sessionFrameFailCount);
  }
  return ok;
}

// ─────────────────────────────────────────────────────────────────────────────

void WsCameraClient::_onEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      _sessionId++;
      _connectedAt          = millis();
      _lastPingAt           = 0;
      _lastPongAt           = 0;
      _lastFrameSentAt      = 0;
      _lastFrameFailAt      = 0;
      _sessionPingCount     = 0;
      _sessionPongCount     = 0;
      _sessionFrameOkCount  = 0;
      _sessionFrameFailCount = 0;

      Serial.printf("[WS] Connected — streaming (session=%u, connect_ms=%lu)\n",
                    _sessionId, millis() - _connectStartedAt);
      _state            = WsState::STREAMING;
      _reconnectDelayMs = 0;
      _connectFailureStreak = 0;
      _lastHeartbeatAt  = millis();
      _nextPingAt       = millis() + CamConfig::kWsPingIntervalMs;
      break;

    case WStype_DISCONNECTED: {
      bool wasConnecting = (_state == WsState::CONNECTING);
      String reason;
      if (payload && length > 0) {
        reason = String(reinterpret_cast<const char*>(payload)).substring(0, length);
      }

      uint16_t code = 0;
      Serial.printf("[WS] Disconnected, close code: %u\n", code);
      if (!reason.isEmpty()) {
        Serial.printf("[WS] Disconnect reason: %s\n", reason.c_str());
      }
      if (code == 0) {
        Serial.println("[WS] No close frame from peer (abnormal transport close)");
      }
      _logDisconnectDiag(code, "disconnected_event");

      if (reason.indexOf("HTTP 401") >= 0) {
        Serial.println("[WS] HTTP 401 during handshake — check deviceApiKey");
        _state = WsState::AUTH_FAILED;
        break;
      }

      if (reason.indexOf("HTTP 403") >= 0) {
        Serial.println("[WS] HTTP 403 during handshake — device probably unclaimed");
        _state = WsState::UNCLAIMED;
        _scheduleReconnect();
        break;
      }

      if (code == WS_CLOSE_AUTH_FAILED) {
        // 4401 — credential/config error: do NOT loop aggressively
        Serial.println("[WS] Auth failed (4401) — check compile-time deviceApiKey");
        _state = WsState::AUTH_FAILED;

      } else if (code == WS_CLOSE_UNCLAIMED) {
        // 4403 — device not yet claimed; retry slowly (user may claim it)
        Serial.println("[WS] Device not claimed (4403) — waiting for claim");
        _state = WsState::UNCLAIMED;
        _scheduleReconnect();

      } else {
        // Normal drop or 4000 (superseded by newer connection) — reconnect normally
        if (code == WS_CLOSE_SUPERSEDED) {
          Serial.println("[WS] Superseded (4000) — reconnecting");
        }
        // Guard: heartbeat timeout already put us in BACKOFF, don't reschedule
        if (_state != WsState::BACKOFF) {
          _scheduleReconnect(wasConnecting);
        }
      }
      break;
    }

    case WStype_PONG:
      _lastPongAt = millis();
      _sessionPongCount++;
      _lastHeartbeatAt = _lastPongAt;
      Serial.printf("[WS] PONG rx #%u (session=%u)\n", _sessionPongCount, _sessionId);
      break;

    case WStype_PING:
      _lastHeartbeatAt = millis();
      Serial.printf("[WS] PING rx (session=%u)\n", _sessionId);
      break;

    case WStype_ERROR:
      Serial.printf("[WS] Socket error — reconnecting (payload_len=%u)\n", length);
      if (payload && length > 0) {
        Serial.printf("[WS] Error payload: %.*s\n", static_cast<int>(length), reinterpret_cast<const char*>(payload));
      }
      _logDisconnectDiag(0, "socket_error");
      _scheduleReconnect(_state == WsState::CONNECTING);
      break;

    default:
      break;
  }
}

void WsCameraClient::_scheduleReconnect(bool fastPath) {
  if (_state == WsState::AUTH_FAILED) return;
  _state = WsState::BACKOFF;

  if (fastPath) {
    _connectFailureStreak++;
    if (_reconnectDelayMs == 0 || _reconnectDelayMs > kFastReconnectMaxMs) {
      _reconnectDelayMs = kFastReconnectBaseMs;
    } else {
      _reconnectDelayMs = min(_reconnectDelayMs + kFastReconnectStepMs, kFastReconnectMaxMs);
    }
  } else {
    _connectFailureStreak = 0;
    _reconnectDelayMs = (_reconnectDelayMs == 0)
                      ? CamConfig::kWsReconnectBaseMs
                      : min(_reconnectDelayMs * 2, CamConfig::kWsReconnectMaxMs);
  }

  Serial.printf("[WS] Reconnecting in %u ms%s\n",
                _reconnectDelayMs,
                fastPath ? " (fast-path)" : "");
  _nextReconnectAt = millis() + _reconnectDelayMs;
}

const char* WsCameraClient::_stateName(WsState state) const {
  switch (state) {
    case WsState::DISCONNECTED: return "DISCONNECTED";
    case WsState::CONNECTING:   return "CONNECTING";
    case WsState::STREAMING:    return "STREAMING";
    case WsState::AUTH_FAILED:  return "AUTH_FAILED";
    case WsState::UNCLAIMED:    return "UNCLAIMED";
    case WsState::BACKOFF:      return "BACKOFF";
    default:                    return "UNKNOWN";
  }
}

int32_t WsCameraClient::_msSince(uint32_t now, uint32_t ts) {
  return (ts == 0) ? -1 : static_cast<int32_t>(now - ts);
}

void WsCameraClient::_logDisconnectDiag(uint16_t code, const char* reason) {
  uint32_t now = millis();
  Serial.printf(
      "[WS][DIAG] reason=%s code=%u state=%s session=%u uptime_ms=%lu conn_age_ms=%d idle_ms=%d since_ping_ms=%d since_pong_ms=%d since_tx_ok_ms=%d since_tx_fail_ms=%d pings=%u pongs=%u tx_ok=%u tx_fail=%u wifi=%d free_heap=%u\n",
      reason,
      code,
      _stateName(_state),
      _sessionId,
      now,
      _msSince(now, _connectedAt),
      _msSince(now, _lastHeartbeatAt),
      _msSince(now, _lastPingAt),
      _msSince(now, _lastPongAt),
      _msSince(now, _lastFrameSentAt),
      _msSince(now, _lastFrameFailAt),
      _sessionPingCount,
      _sessionPongCount,
      _sessionFrameOkCount,
      _sessionFrameFailCount,
      WiFi.status(),
      ESP.getFreeHeap());
}
