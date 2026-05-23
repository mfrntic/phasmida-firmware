#pragma once

#include <Arduino.h>
#include <functional>
#include <led/LedManager.h>

// RgbVerificationManager
// ----------------------
// Manages the RGB Soft Hotplug Verification session state machine.
//
//   States: Idle → Pending → (Idle after result)
//
//   Usage:
//     g_rgbVerification.begin(g_ledMgr, onResult);  // once in setup()
//     g_rgbVerification.service(millis());           // every loop()
//     g_rgbVerification.handleCommand(...);          // in MQTT command handler
//     g_rgbVerification.onConfirm();                 // from UI DA button
//     g_rgbVerification.onReject();                  // from UI NE button
class RgbVerificationManager {
public:
  // Called when a session ends; result = "confirmed"|"rejected"|"timeout",
  // reason = e.g. "user_confirmed", "confirm_timeout", "session_replaced"
  using ResultCb = std::function<void(
    const String& sessionId,
    const char*   result,
    const char*   reason,
    uint32_t      durationMs,
    uint32_t      confirmWindowMs,
    const char*   pattern
  )>;

  void begin(LedManager& ledMgr, ResultCb cb);

  // Parse and validate the start-rgb-verification command params.
  // Sets ackOk=true on success (session started), false on reject.
  // On reject, ackError contains the error.code string.
  void handleCommand(
    const String& cmdId,
    const String& sessionId,
    const String& pattern,
    uint32_t      durationMs,
    uint32_t      confirmWindowMs,
    bool&         ackOk,
    String&       ackError
  );

  void onConfirm();   // user pressed DA
  void onReject();    // user pressed NE
  void service(uint32_t nowMs);

  bool     isPending()                        const { return _state == State::Pending; }
  String   activeSessionId()                  const { return _sessionId; }
  uint32_t remainingConfirmMs(uint32_t nowMs) const;

private:
  enum class State { Idle, Pending };

  void _complete(const char* result, const char* reason);

  LedManager* _ledMgr       = nullptr;
  ResultCb    _resultCb;
  State       _state         = State::Idle;

  String   _sessionId;
  String   _pattern;
  uint32_t _durationMs       = 0;
  uint32_t _confirmWindowMs  = 0;
  uint32_t _pendingSinceMs   = 0;   // millis() when session entered Pending

  uint32_t _completedAtMs    = 0;   // millis() when last session ended (cooldown)
  bool     _hasCooldown      = false;
};
