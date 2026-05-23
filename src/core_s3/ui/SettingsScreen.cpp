#include "ui/SettingsScreen.h"
#include "ui/ScreenManager.h"
#include "ui/BootLogScreen.h"
#include "ui/UICommon.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <app_config.h>

SettingsScreen::SettingsScreen(ScreenManager& mgr, BootLogScreen& bootLog)
  : _mgr(mgr), _bootLog(bootLog) {}

// ---------------------------------------------------------------------------

void SettingsScreen::setNavInfo(int myIdx, int total) {
  _myIndex      = myIdx;
  _totalScreens = total;
}

// ---------------------------------------------------------------------------

void SettingsScreen::onEnter() {
  _myIndex      = _mgr.activeIndex();
  _totalScreens = _mgr.screenCount();
  _confirmWifiReset = false;
  draw();
}

void SettingsScreen::draw() {
  _render(M5.Display, _totalScreens, _myIndex);
}

void SettingsScreen::drawIntoSprite(LGFX_Sprite& sp) {
  _render(sp, _totalScreens, _myIndex);
}

// ---------------------------------------------------------------------------

void SettingsScreen::setWifiResetCallback(std::function<void()> cb) {
  _wifiResetCb = cb;
}

void SettingsScreen::setWifiConfiguredCallback(std::function<bool()> cb) {
  _wifiConfiguredCb = cb;
}

void SettingsScreen::onVerticalTouch(int32_t /*x*/, int32_t y) {
  if (y >= kBtnY1 && y <= kBtnY2) {
    _confirmWifiReset = false;
    _openBootLog();
  } else if (y >= kWifiBtnY1 && y <= kWifiBtnY2) {
    _triggerWifiReset();
  } else if (_confirmWifiReset) {
    _confirmWifiReset = false;
    draw();
  }
}

void SettingsScreen::onBtnB() {
  _openBootLog();
}

// ---------------------------------------------------------------------------

void SettingsScreen::_openBootLog() {
  _mgr.showTransient(&_bootLog);
}

void SettingsScreen::_triggerWifiReset() {
  if (_requiresWifiResetConfirmation() && !_confirmWifiReset) {
    _confirmWifiReset = true;
    draw();
    return;
  }

  _confirmWifiReset = false;
  if (_wifiResetCb) {
    _wifiResetCb();
  }
}

bool SettingsScreen::_requiresWifiResetConfirmation() const {
  return _wifiConfiguredCb ? _wifiConfiguredCb() : false;
}

// ---------------------------------------------------------------------------

template<typename GFX>
void SettingsScreen::_render(GFX& gfx, int totalScreens, int myIndex) {
  gfx.fillScreen(TFT_BLACK);

  // ---- Title ----
  gfx.setFont(&lgfx::fonts::FreeSansBold12pt7b);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::top_center);
  gfx.drawString("Phasmida Core", 160, 18);

  // ---- Version ----
  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextColor(0x8410U /* dark grey */, TFT_BLACK); // RGB565 ~50% grey
  gfx.drawString(String("v") + AppConfig::kFwVersion, 160, 50);

  // ---- MAC ----
  gfx.setFont(nullptr); // default small font
  gfx.setTextSize(1);
  gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::top_left);
  String mac = WiFi.macAddress();
  gfx.setCursor(10, 82);
  gfx.print("MAC: ");
  gfx.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  gfx.println(mac);

  // ---- Boot Log button ----
  gfx.fillRoundRect(20, kBtnY1, 280, kBtnY2 - kBtnY1, 8, 0x2104U /* very dark grey */);
  gfx.drawRoundRect(20, kBtnY1, 280, kBtnY2 - kBtnY1, 8, 0x4208U /* mid grey border */);
  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.drawString("Boot Log", 160, kBtnY1 + (kBtnY2 - kBtnY1) / 2);

  // ---- Wi-Fi Setup button ----
  gfx.fillRoundRect(20, kWifiBtnY1, 280, kWifiBtnY2 - kWifiBtnY1, 8, 0x180EU /* very dark red-tint */);
  gfx.drawRoundRect(20, kWifiBtnY1, 280, kWifiBtnY2 - kWifiBtnY1, 8, 0x4208U /* mid grey border */);
  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextColor(0xFD20U /* amber/orange */, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.drawString(_confirmWifiReset ? "Confirm Wi-Fi Reset" : "Wi-Fi Setup", 160,
                 kWifiBtnY1 + (kWifiBtnY2 - kWifiBtnY1) / 2);

  if (_confirmWifiReset) {
    gfx.setFont(nullptr);
    gfx.setTextSize(1);
    gfx.setTextColor(TFT_DARKGREY, TFT_BLACK);
    gfx.setTextDatum(textdatum_t::top_center);
    gfx.drawString("Tap again to forget saved Wi-Fi", 160, 220);
  }

  // ---- Carousel dots ----
  ui::drawCarouselDots(gfx, totalScreens, myIndex);

  gfx.setTextDatum(textdatum_t::top_left);
}

template void SettingsScreen::_render<M5GFX>(M5GFX&, int, int);
template void SettingsScreen::_render<LGFX_Sprite>(LGFX_Sprite&, int, int);
