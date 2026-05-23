#include "ui/RgbLightScreen.h"
#include "ui/UICommon.h"
#include "led/LedManager.h"
#include "ConfigStore.h"

#include <M5Unified.h>

// ─────────────────────────────────────────────────────────────────────────────

RgbLightScreen::RgbLightScreen(LedManager& ledMgr, ConfigStore& configStore)
  : _ledMgr(ledMgr), _configStore(configStore) {}

// ─────────────────────────────────────────────────────────────────────────────

void RgbLightScreen::setNavInfo(int myIdx, int total) {
  _myIndex      = myIdx;
  _totalScreens = total;
}

void RgbLightScreen::onEnter() {
  _lastRgbOn    = _ledMgr.isLightActive();
  _lastBottomOn = _ledMgr.isBottomStripActive();
  draw();
}

void RgbLightScreen::draw() {
  _render(M5.Display);
}

void RgbLightScreen::drawIntoSprite(LGFX_Sprite& sp) {
  _render(sp);
}

// ─────────────────────────────────────────────────────────────────────────────

template<typename GFX>
void RgbLightScreen::_render(GFX& gfx) {
  gfx.fillScreen(TFT_BLACK);

  // Left vertical title rail (matches sensor screen style)
  constexpr int16_t kRailCenterX = 12;
  constexpr int16_t kRailTopY    = 24;
  constexpr int16_t kRailBottomY = 214;
  constexpr int16_t kDividerX    = 24;
  constexpr int16_t kDividerY    = 20;
  constexpr int16_t kDividerH    = 196;

  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextSize(1);
  gfx.setTextColor(0xC618U, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::middle_center);

  const char* title = "LED";
  const int16_t kCharStep = gfx.fontHeight() + 2;
  int16_t totalH = 0;
  for (const char* p = title; *p; ++p) totalH += kCharStep;
  if (totalH > 0) totalH -= kCharStep;
  const int16_t railH = (kRailBottomY - kRailTopY);
  int16_t titleY = kRailTopY + ((railH - totalH) / 2);
  for (const char* p = title; *p; ++p) {
    char ch[2] = {*p, '\0'};
    gfx.drawString(ch, kRailCenterX, titleY);
    titleY += kCharStep;
  }

  gfx.drawFastVLine(kDividerX, kDividerY, kDividerH, 0x4208U);

  constexpr int16_t kContentX = 30;
  constexpr int16_t kContentW = 280;
  constexpr int16_t kContentCenterX = kContentX + (kContentW / 2);

  bool rgbOn    = _ledMgr.isLightActive();
  bool bottomOn = _ledMgr.isBottomStripActive();

  // RGB Unit toggle button  (green accent when on, dark when off)
  uint16_t rgbBg  = rgbOn ? 0x0320U : 0x2104U;
  uint16_t rgbBdr = rgbOn ? TFT_GREEN : 0x4208U;
  gfx.fillRoundRect(kContentX, kRgbBtnY1, kContentW, kRgbBtnY2 - kRgbBtnY1, 8, rgbBg);
  gfx.drawRoundRect(kContentX, kRgbBtnY1, kContentW, kRgbBtnY2 - kRgbBtnY1, 8, rgbBdr);
  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.drawString(rgbOn ? "RGB Unit: ON" : "RGB Unit: OFF",
                 kContentCenterX, (kRgbBtnY1 + kRgbBtnY2) / 2);

  // Bottom strip toggle button (cyan accent when on, dark when off)
  uint16_t btmBg  = bottomOn ? 0x0318U : 0x2104U;
  uint16_t btmBdr = bottomOn ? 0x07FFU : 0x4208U;
  gfx.fillRoundRect(kContentX, kBottomBtnY1, kContentW, kBottomBtnY2 - kBottomBtnY1, 8, btmBg);
  gfx.drawRoundRect(kContentX, kBottomBtnY1, kContentW, kBottomBtnY2 - kBottomBtnY1, 8, btmBdr);
  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextColor(TFT_WHITE, TFT_BLACK);
  gfx.setTextDatum(textdatum_t::middle_center);
  gfx.drawString(bottomOn ? "Bottom Strip: ON" : "Bottom Strip: OFF",
                 kContentCenterX, (kBottomBtnY1 + kBottomBtnY2) / 2);

  // Nav dots
  ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
}

// ─────────────────────────────────────────────────────────────────────────────

void RgbLightScreen::onUpdate() {
  bool rgbOn    = _ledMgr.isLightActive();
  bool bottomOn = _ledMgr.isBottomStripActive();
  if (rgbOn != _lastRgbOn || bottomOn != _lastBottomOn) {
    _lastRgbOn    = rgbOn;
    _lastBottomOn = bottomOn;
    draw();
  }
}

// ───────────────────────────────────────────────────────────────────────────────

void RgbLightScreen::onVerticalTouch(int32_t /*x*/, int32_t y) {
  if (y >= kRgbBtnY1 && y <= kRgbBtnY2) {
    _toggleRgbUnit();
  } else if (y >= kBottomBtnY1 && y <= kBottomBtnY2) {
    _toggleBottomStrip();
  }
}

// ─────────────────────────────────────────────────────────────────────────────

void RgbLightScreen::_toggleRgbUnit() {
  if (_ledMgr.isLightActive()) {
    // Local OFF = temporary — keep NVS intact so cloud color survives reboot
    _ledMgr.setRgbUnitOff();
  } else {
    // Prefer in-memory state (LedManager keeps _toColor/_targetBrightness even when off)
    String cmdId = _ledMgr.lastSetLightCmdId();
    if (cmdId.length() > 0) {
      // Restore exact previous color (cloud or prior local-on)
      _ledMgr.applySetLight(_ledMgr.activeColor(), _ledMgr.activeBrightness(), cmdId);
    } else {
      // First boot with no cloud command yet — fall back to NVS or white@200
      PersistedLightState ls = _configStore.loadLightState();
      CRGB    color      = ls.valid ? ls.color      : CRGB::White;
      uint8_t brightness = ls.valid ? ls.brightness : static_cast<uint8_t>(200);
      String  newCmdId("local-on");
      _ledMgr.applySetLight(color, brightness, newCmdId);
      if (!ls.valid) {
        _configStore.saveLightState(color, brightness, newCmdId);
      }
    }
  }
  draw();
}

void RgbLightScreen::_toggleBottomStrip() {
  if (_ledMgr.isBottomStripActive()) {
    _ledMgr.setOff();
  } else {
    _ledMgr.applyCommand("solid", "#FFFFFF", 0);
  }
  draw();
}
