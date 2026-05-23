#include "ui/SoilMoistureScreen.h"
#include "ui/UICommon.h"
#include <M5Unified.h>

SoilMoistureScreen::SoilMoistureScreen() {
  _cardMoisture.setLabel("SOIL MOISTURE");
  _cardMoisture.setUnit("%");
  _cardMoisture.setValueColor(TFT_GREEN);
  _cardMoisture.setPrecision(1);
  _cardMoisture.setNoBorder();
}

void SoilMoistureScreen::notifyNewReadings(float moisturePct) {
  _cardMoisture.setValue(moisturePct);
  if (_active) draw();
}

void SoilMoistureScreen::setNavInfo(int myIdx, int total) {
  if (_myIndex != myIdx || _totalScreens != total) {
    _needsFullClear = true;
  }
  _myIndex = myIdx;
  _totalScreens = total;
}

void SoilMoistureScreen::onEnter() {
  _active = true;
  _needsFullClear = true;
  draw();
}

void SoilMoistureScreen::onExit() {
  _active = false;
}

void SoilMoistureScreen::draw() {
  _render(M5.Display, false);
  _lastDrawMs = millis();
}

void SoilMoistureScreen::drawIntoSprite(LGFX_Sprite& sp) {
  bool savedNFC = _needsFullClear;
  _needsFullClear = true;
  _render(sp, true);
  _needsFullClear = savedNFC;
}

void SoilMoistureScreen::onUpdate() {
  if (!_active) return;
  if (millis() - _lastDrawMs >= kDrawIntervalMs) {
    draw();
  }
}

void SoilMoistureScreen::onBtnB() {
  // Reserved for future use
}

template<typename GFX>
void SoilMoistureScreen::_render(GFX& gfx, bool /*forceFull*/) {
  bool didFullClear = _needsFullClear;

  if (didFullClear) {
    gfx.fillScreen(TFT_BLACK);
    _needsFullClear = false;

    // ---- Probe title (vertical left rail, anti-aliased font) ----
    gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
    gfx.setTextSize(1);
    gfx.setTextColor(0xC618U /* light grey */, TFT_BLACK);
    gfx.setTextDatum(textdatum_t::middle_center);

    constexpr int16_t kRailCenterX = 12;
    constexpr int16_t kRailTopY = 24;
    constexpr int16_t kRailBottomY = 214;
    const int16_t kCharStep = gfx.fontHeight() + 2;

    const char* title = "SOIL";
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
    // Subtle vertical divider between title rail and card.
    gfx.drawFastVLine(24, 20, 196, 0x4208U /* dim grey */);
  }

  // ---- Card ----
  if (didFullClear || _cardMoisture.isDirty()) {
    _cardMoisture.render(gfx);
  }

  // ---- Carousel dots (only on full clear) ----
  if (didFullClear) {
    ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
  }

  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(nullptr);
}

template void SoilMoistureScreen::_render<M5GFX>(M5GFX&, bool);
template void SoilMoistureScreen::_render<LGFX_Sprite>(LGFX_Sprite&, bool);
