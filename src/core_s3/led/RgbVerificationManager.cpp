#include <led/RgbVerificationManager.h>
#include <app_config.h>

void RgbVerificationManager::begin(LedManager& ledMgr, ResultCb cb) {
  _ledMgr   = &ledMgr;
  _resultCb = cb;
}

void RgbVerificationManager::handleCommand(
    const String& cmdId,
    const String& sessionId,
    const String& pattern,
    uint32_t      durationMs,
    uint32_t      confirmWindowMs,
    bool&         ackOk,
    String&       ackError) {

  (void)cmdId;  // dedup handled by main.cpp before this call

  // ── Parameter validation ──────────────────────────────────────────────────
  if (sessionId.isEmpty() || sessionId.length() > 128) {
    ackOk = false; ackError = "invalid_session_id"; return;
  }
  if (pattern != "discovery_rgb") {
    ackOk = false; ackError = "invalid_pattern"; return;
  }
  if (durationMs < 1000 || durationMs > 15000) {
    ackOk = false; ackError = "invalid_duration_ms"; return;
  }
  if (confirmWindowMs < 5000 || confirmWindowMs > 60000) {
    ackOk = false; ackError = "invalid_confirm_window_ms"; return;
  }

  uint32_t nowMs = millis();

  // ── Cooldown check (not applied when replacing a pending session) ─────────
  if (_state == State::Idle && _hasCooldown) {
    if ((nowMs - _completedAtMs) < AppConfig::kRgbVerifyCooldownMs) {
      ackOk = false; ackError = "session_cooldown"; return;
    }
  }

  // ── Session replace: close existing pending session ───────────────────────
  if (_state == State::Pending) {
    // Close old session with session_replaced; no cooldown penalty
    if (_ledMgr) _ledMgr->endVerificationPattern();
    if (_resultCb) {
      _resultCb(_sessionId, "timeout", "session_replaced",
                _durationMs, _confirmWindowMs, _pattern.c_str());
    }
    _state = State::Idle;
  }

  // ── Start new session ─────────────────────────────────────────────────────
  _sessionId       = sessionId;
  _pattern         = pattern;
  _durationMs      = durationMs;
  _confirmWindowMs = confirmWindowMs;
  _pendingSinceMs  = nowMs;
  _state           = State::Pending;

  // Pokušaj pokrenuti verification pattern
  if (_ledMgr) {
    if (!_ledMgr->beginVerificationPattern()) {
      ackOk = false;
      ackError = "led_driver_unavailable";
      return;
    }
  }

  ackOk = true;
}

void RgbVerificationManager::onConfirm() {
  if (_state != State::Pending) return;
  _complete("confirmed", "user_confirmed");
}

void RgbVerificationManager::onReject() {
  if (_state != State::Pending) return;
  _complete("rejected", "user_rejected");
}

void RgbVerificationManager::service(uint32_t nowMs) {
  if (_state != State::Pending) return;
  if ((nowMs - _pendingSinceMs) >= _confirmWindowMs) {
    _complete("timeout", "confirm_timeout");
  }
}

uint32_t RgbVerificationManager::remainingConfirmMs(uint32_t nowMs) const {
  if (_state != State::Pending) return 0;
  uint32_t elapsed = nowMs - _pendingSinceMs;
  if (elapsed >= _confirmWindowMs) return 0;
  return _confirmWindowMs - elapsed;
}

void RgbVerificationManager::_complete(const char* result, const char* reason) {
  if (_ledMgr) _ledMgr->endVerificationPattern();

  String sid        = _sessionId;
  uint32_t dur      = _durationMs;
  uint32_t confWin  = _confirmWindowMs;
  String pat        = _pattern;

  _state          = State::Idle;
  _completedAtMs  = millis();
  _hasCooldown    = true;
  _sessionId      = "";

  if (_resultCb) {
    _resultCb(sid, result, reason, dur, confWin, pat.c_str());
  }
}
