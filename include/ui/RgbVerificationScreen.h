#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include <functional>
#include "IScreen.h"

// RgbVerificationScreen
// ---------------------
// Transient/modal IScreen shown during an RGB Soft Hotplug Verification session.
// Displays title, message, DA/NE touch buttons, and a live countdown.
//
// Lifecycle:
//   screen.setSession(sessionId, confirmWindowMs, millis());
//   g_screenMgr.showTransient(&screen);
//   // ... user interacts or timeout fires -> callbacks -> g_screenMgr.dismissTransient()
class RgbVerificationScreen : public IScreen {
public:
  void setSession(const String& sessionId, uint32_t confirmWindowMs, uint32_t startMs);
  void clearSession();

  void setConfirmCallback(std::function<void()> cb) { _confirmCb = cb; }
  void setRejectCallback(std::function<void()>  cb) { _rejectCb  = cb; }

  void onEnter()                            override;
  void draw()                               override;
  void drawIntoSprite(LGFX_Sprite& sp)      override;
  void onUpdate()                           override;
  void onVerticalTouch(int32_t x, int32_t y) override;

private:
  template<typename GFX>
  void _render(GFX& gfx);
  void _refreshCountdown();

  uint32_t _remainingSecs() const;

  String   _sessionId;
  uint32_t _confirmWindowMs = 15000;
  uint32_t _startMs         = 0;
  uint32_t _lastDrawnSecs   = 0xFFFFFFFFu;

  std::function<void()> _confirmCb;
  std::function<void()> _rejectCb;

  // Touch zones (full-width halves for DA/NE)
  static constexpr int32_t kBtnTop    = 150;
  static constexpr int32_t kBtnBottom = 220;
  static constexpr int32_t kMidX      = 160;
};
