#include "ui/DLightScreen.h"

#include <M5Unified.h>

#include <app_config.h>
#include "ui/UICommon.h"

namespace {
enum class TerrariumLightBand { Low, Med, High };

TerrariumLightBand classifyLux(float lux) {
  if (lux < AppConfig::kTerrariumLuxLowMax) {
    return TerrariumLightBand::Low;
  }
  if (lux <= AppConfig::kTerrariumLuxMedMax) {
    return TerrariumLightBand::Med;
  }
  return TerrariumLightBand::High;
}
}  // namespace

DLightScreen::DLightScreen() {
  _cardLux.setLabel("LIGHT");
  _cardLux.setUnit("lx");
  _cardLux.setValueColor(TFT_YELLOW);
  _cardLux.setPrecision(0);
  _cardLux.setNoBorder();
}

void DLightScreen::notifyNewReadings(float lux) {
  switch (classifyLux(lux)) {
    case TerrariumLightBand::Low:
      _cardLux.setLabel("LIGHT LOW");
      _cardLux.setValueColor(TFT_ORANGE);
      break;
    case TerrariumLightBand::Med:
      _cardLux.setLabel("LIGHT MED");
      _cardLux.setValueColor(TFT_GREEN);
      break;
    case TerrariumLightBand::High:
      _cardLux.setLabel("LIGHT HIGH");
      _cardLux.setValueColor(TFT_RED);
      break;
  }
  _cardLux.setValue(lux);
  if (_active) draw();
}

void DLightScreen::setNavInfo(int myIdx, int total) {
  if (_myIndex != myIdx || _totalScreens != total) {
    _needsFullClear = true;
  }
  _myIndex = myIdx;
  _totalScreens = total;
}

void DLightScreen::onEnter() {
  _active = true;
  _needsFullClear = true;
  draw();
}

void DLightScreen::onExit() {
  _active = false;
}

void DLightScreen::draw() {
  _render(M5.Display, false);
  _lastDrawMs = millis();
}

void DLightScreen::drawIntoSprite(LGFX_Sprite& sp) {
  bool savedNFC = _needsFullClear;
  _needsFullClear = true;
  _render(sp, true);
  _needsFullClear = savedNFC;
}

void DLightScreen::onUpdate() {
  if (!_active) return;
  if (millis() - _lastDrawMs >= kDrawIntervalMs) {
    draw();
  }
}

void DLightScreen::onBtnB() {
  // Reserved for future use
}

template<typename GFX>
void DLightScreen::_render(GFX& gfx, bool /*forceFull*/) {
  bool didFullClear = _needsFullClear;

  if (didFullClear) {
    gfx.fillScreen(TFT_BLACK);
    _needsFullClear = false;

    gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
    gfx.setTextSize(1);
    gfx.setTextColor(0xC618U /* light grey */, TFT_BLACK);
    gfx.setTextDatum(textdatum_t::middle_center);

    constexpr int16_t kRailCenterX = 12;
    constexpr int16_t kRailTopY = 24;
    constexpr int16_t kRailBottomY = 214;
    const int16_t kCharStep = gfx.fontHeight() + 2;

    const char* title = "LIGHT";
    int16_t totalH = 0;
    for (const char* p = title; *p; ++p) {
      totalH += kCharStep;
    }
    if (totalH > 0) {
      totalH -= kCharStep;
    }

    const int16_t railH = (kRailBottomY - kRailTopY);
    int16_t titleY = kRailTopY + ((railH - totalH) / 2);

    for (const char* p = title; *p; ++p) {
      char ch[2] = {*p, '\0'};
      gfx.drawString(ch, kRailCenterX, titleY);
      titleY += kCharStep;
    }

    gfx.setTextDatum(textdatum_t::top_left);
    gfx.drawFastVLine(24, 20, 196, 0x4208U /* dim grey */);
  }

  if (didFullClear || _cardLux.isDirty()) {
    _cardLux.render(gfx);
  }

  if (didFullClear) {
    ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
  }

  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(nullptr);
}

template void DLightScreen::_render<M5GFX>(M5GFX&, bool);
template void DLightScreen::_render<LGFX_Sprite>(LGFX_Sprite&, bool);
