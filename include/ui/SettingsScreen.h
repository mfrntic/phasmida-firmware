#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include <functional>
#include "IScreen.h"

class ScreenManager;
class BootLogScreen;

class SettingsScreen : public IScreen {
public:
  SettingsScreen(ScreenManager& mgr, BootLogScreen& bootLog);

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void draw()    override;
  void setWifiResetCallback(std::function<void()> cb);
  void setWifiConfiguredCallback(std::function<bool()> cb);
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onVerticalTouch(int32_t x, int32_t y) override;
  void onBtnB() override;

private:
  template<typename GFX>
  void _render(GFX& gfx, int totalScreens, int myIndex);

  void _openBootLog();
  void _triggerWifiReset();
  bool _requiresWifiResetConfirmation() const;

  ScreenManager& _mgr;
  BootLogScreen& _bootLog;
  std::function<void()> _wifiResetCb;
  std::function<bool()> _wifiConfiguredCb;
  bool _confirmWifiReset = false;

  int _totalScreens = 0;
  int _myIndex      = 0;

  // Touch targets
  static constexpr int32_t kBtnY1     = 128;
  static constexpr int32_t kBtnY2     = 168;
  static constexpr int32_t kWifiBtnY1 = 176;
  static constexpr int32_t kWifiBtnY2 = 216;
};
