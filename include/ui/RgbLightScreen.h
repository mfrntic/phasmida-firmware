#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include "IScreen.h"

class LedManager;
class ConfigStore;

// RgbLightScreen
// --------------
// Carousel screen for on-device LED control (no cloud required).
//
// Two toggle buttons:
//   • RGB Unit  (SK6812, GPIO17) — same strip as the set-light command
//   • Bottom Strip (WS2812, GPIO5) — M5GO3 base module LEDs
//
// RGB Unit toggle:
//   OFF → turns off + clears NVS (so reboot keeps it off)
//   ON  → restores last NVS-persisted color/brightness, or white@200 if none
//
// Bottom Strip toggle:
//   OFF → solid off
//   ON  → solid white, no expiry
class RgbLightScreen : public IScreen {
public:
  RgbLightScreen(LedManager& ledMgr, ConfigStore& configStore);

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void draw()    override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onVerticalTouch(int32_t x, int32_t y) override;
  void onUpdate() override;

private:
  template<typename GFX>
  void _render(GFX& gfx);

  void _toggleRgbUnit();
  void _toggleBottomStrip();

  LedManager&  _ledMgr;
  ConfigStore& _configStore;

  int _totalScreens = 0;
  int _myIndex      = 0;

  // Cached state for onUpdate() diff — avoids redraw every loop tick
  bool _lastRgbOn    = false;
  bool _lastBottomOn = false;

  // Touch / draw zones (Y coordinates, 320×240 display)
  static constexpr int32_t kRgbBtnY1    =  76;
  static constexpr int32_t kRgbBtnY2    = 116;
  static constexpr int32_t kBottomBtnY1 = 132;
  static constexpr int32_t kBottomBtnY2 = 172;
};
