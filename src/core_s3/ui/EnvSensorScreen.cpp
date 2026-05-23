#include "ui/EnvSensorScreen.h"
#include "ui/UICommon.h"
#include <M5Unified.h>

EnvSensorScreen::EnvSensorScreen(const char* probeTitle)
    : _probeTitle((probeTitle && probeTitle[0]) ? probeTitle : "ENV") {
  _configureCards();
}

void EnvSensorScreen::_configureCards() {
  // Proporcionalno popuni vertikalni prostor (y=24..214, h=190) između
  // gornje navigacije i donjih carousel-točkica.
  ui::MeasurementCard* cards[] = { &_cardTemp, &_cardHum, &_cardPress };
  ui::MeasurementCard::layoutVertical(cards, 3, /*x*/30, /*y*/24, /*w*/282, /*h*/190, /*gap*/4);

  _cardTemp.setLabel("TEMPERATURE");
  _cardTemp.setUnit("\xB0""C");
  _cardTemp.setValueColor(TFT_RED);
  _cardTemp.setPrecision(2);
  _cardTemp.setNoBorder();

  _cardHum.setLabel("HUMIDITY");
  _cardHum.setUnit("%");
  _cardHum.setValueColor(TFT_CYAN);
  _cardHum.setPrecision(2);
  _cardHum.setNoBorder();

  _cardPress.setLabel("ATMOSPHERIC PRESSURE");
  _cardPress.setUnit("hPa");
  _cardPress.setValueColor(TFT_YELLOW);
  _cardPress.setPrecision(2);
  _cardPress.setNoBorder();
}

// ---------------------------------------------------------------------------

void EnvSensorScreen::notifyNewReadings(float temp, float hum, float pressPa) {
  _cardTemp.setValue(temp);
  _cardHum.setValue(hum);
  _cardPress.setValue(pressPa / 100.0f);
  if (_active) draw();
}
void EnvSensorScreen::setNavInfo(int myIdx, int total) {
  if (_myIndex != myIdx || _totalScreens != total) {
    _needsFullClear = true;
  }
  _myIndex      = myIdx;
  _totalScreens = total;
}

void EnvSensorScreen::onExit() {
  _active = false;
}

void EnvSensorScreen::onEnter() {
  _active         = true;
  _needsFullClear = true;
  draw();  // draw layout immediately; values fill in as samples arrive
}

void EnvSensorScreen::draw() {
  // Always render the layout (title rail + cards). When no data is available
  // yet the cards display "--", which is preferable to leaving the screen
  // blank right after the boot transition.
  _render(M5.Display, false);
  _lastDrawMs = millis();
}

void EnvSensorScreen::drawIntoSprite(LGFX_Sprite& sp) {
  // Always force a full render into the sprite (don't disturb display delta state).
  bool savedNFC = _needsFullClear;
  _needsFullClear = true;
  _render(sp, true);
  _needsFullClear = savedNFC;
}

void EnvSensorScreen::onUpdate() {
  if (!_active) return;
  if (millis() - _lastDrawMs >= kDrawIntervalMs) {
    draw();
  }
}

void EnvSensorScreen::onBtnB() {
  // Reserved for future use (e.g. unit toggle)
}

// ---------------------------------------------------------------------------

template<typename GFX>
void EnvSensorScreen::_render(GFX& gfx, bool /*forceFull*/) {
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
    constexpr int16_t kRailTopY    = 24;
    constexpr int16_t kRailBottomY = 214;
    const int16_t kCharStep = gfx.fontHeight() + 2;
    const int16_t kWordGap  = kCharStep / 2;

    int16_t totalH = 0;
    for (const char* p = _probeTitle; *p; ++p) {
      totalH += (*p == ' ') ? kWordGap : kCharStep;
    }
    if (totalH > 0) {
      totalH -= kCharStep;
    }

    const int16_t railH = (kRailBottomY - kRailTopY);
    int16_t titleY = kRailTopY + ((railH - totalH) / 2);

    for (const char* p = _probeTitle; *p; ++p) {
      if (*p == ' ') {
        titleY += kWordGap;
        continue;
      }
      char ch[2] = {*p, '\0'};
      gfx.drawString(ch, kRailCenterX, titleY);
      titleY += kCharStep;
    }

    gfx.setTextDatum(textdatum_t::top_left);
    // Subtle vertical divider between title rail and cards.
    gfx.drawFastVLine(24, 20, 196, 0x4208U /* dim grey */);
  }

  // ---- Cards ----
  if (didFullClear || _cardTemp.isDirty())  _cardTemp.render(gfx);
  if (didFullClear || _cardHum.isDirty())   _cardHum.render(gfx);
  if (didFullClear || _cardPress.isDirty()) _cardPress.render(gfx);

  // ---- Carousel dots (only on full clear; cards never touch y>=218) ----
  if (didFullClear) {
    ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
  }

  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(nullptr);
}

template void EnvSensorScreen::_render<M5GFX>(M5GFX&, bool);
template void EnvSensorScreen::_render<LGFX_Sprite>(LGFX_Sprite&, bool);
