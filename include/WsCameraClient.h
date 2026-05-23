#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>

// WS close codes defined by the backend contract
static constexpr uint16_t WS_CLOSE_AUTH_FAILED  = 4401;
static constexpr uint16_t WS_CLOSE_UNCLAIMED    = 4403;
static constexpr uint16_t WS_CLOSE_SUPERSEDED   = 4000;

enum class WsState : uint8_t {
  DISCONNECTED,
  CONNECTING,
  STREAMING,
  AUTH_FAILED,  // 4401 — credential error; human intervention needed
  UNCLAIMED,    // 4403 — device not yet claimed by user
  BACKOFF,      // waiting before next reconnect attempt
};

class WsCameraClient {
public:
  void begin(const String& host, uint16_t port,
             const String& slug, const String& apiKey,
             bool useTls = true);

  // Drive WS event loop, ping/pong tracking, reconnect scheduling.
  // Call every loop() iteration.
  void loop();
  void disconnect() { _ws.disconnect(); }

  // Send one complete JPEG frame as a binary WS message.
  // Returns false if not currently streaming.
  bool sendFrame(const uint8_t* data, size_t len);

  // Reset heartbeat timer without sending data.
  // Call before any blocking operation (e.g. captureFrame) to prevent
  // spurious heartbeat timeouts while the loop() is not being serviced.
  void touchHeartbeat();

  WsState state()      const { return _state; }
  bool    isStreaming() const { return _state == WsState::STREAMING; }

private:
  WebSocketsClient _ws;
  WsState  _state = WsState::DISCONNECTED;

  String   _host;
  uint16_t _port            = 443;
  String   _slug;
  String   _apiKey;
  bool     _useTls          = true;

  uint32_t _nextPingAt      = 0;
  uint32_t _lastHeartbeatAt = 0;   // updated on pong/ping receipt and frame send
  uint32_t _nextReconnectAt = 0;
  uint32_t _reconnectDelayMs = 0;

  uint32_t _connectAttemptCount = 0;
  uint32_t _sessionId           = 0;
  uint32_t _connectStartedAt    = 0;
  uint32_t _connectedAt         = 0;
  uint32_t _lastPingAt          = 0;
  uint32_t _lastPongAt          = 0;
  uint32_t _lastFrameSentAt     = 0;
  uint32_t _lastFrameFailAt     = 0;
  uint32_t _sessionPingCount    = 0;
  uint32_t _sessionPongCount    = 0;
  uint32_t _sessionFrameOkCount = 0;
  uint32_t _sessionFrameFailCount = 0;
  uint32_t _connectFailureStreak = 0;

  void _connect();
  void _onEvent(WStype_t type, uint8_t* payload, size_t length);
  void _scheduleReconnect(bool fastPath = false);
  const char* _stateName(WsState state) const;
  void _logDisconnectDiag(uint16_t code, const char* reason);
  static int32_t _msSince(uint32_t now, uint32_t ts);

  static WsCameraClient* _instance;
  static void _staticEvent(WStype_t type, uint8_t* payload, size_t length);
};
