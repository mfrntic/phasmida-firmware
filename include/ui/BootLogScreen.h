#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include "IScreen.h"

// Forward-declare ScreenManager to avoid circular include
class ScreenManager;

class BootLogScreen : public IScreen {
public:
  // Must be set before onBtnB() is used in transient mode
  void setScreenManager(ScreenManager* mgr) { _mgr = mgr; }

  void pushLine(const String& line);

  void onEnter() override;
  void onExit()  override;
  void draw()    override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onVerticalTouch(int32_t x, int32_t y) override;
  void onBtnB() override;

private:
  template<typename GFX>
  void _render(GFX& gfx);

  uint16_t _maxViewStart() const;

  static constexpr uint16_t kMaxLines     = 160;
  static constexpr uint8_t  kVisibleLines = 13;
  static constexpr int16_t  kLineHeight   = 16;
  static constexpr int16_t  kStartY       = 4;
  static constexpr uint8_t  kScrollStep   = 4;

  String   _logLines[kMaxLines];
  uint16_t _logCount    = 0;
  uint16_t _viewStart   = 0;
  bool     _followTail  = true;
  bool     _active      = false;

  ScreenManager* _mgr = nullptr;
};
