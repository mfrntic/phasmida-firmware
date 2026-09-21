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

void SoilMoistureScreen::notifyNewReadings(float moisturePct, uint16_t milliVolts) {
  _cardMoisture.setValue(moisturePct);
  if (_milliVolts != milliVolts || !_hasMilliVolts) {
    _milliVolts    = milliVolts;
    _hasMilliVolts = true;
    _infoDirty     = true;
  }
  if (_active) draw();
}

void SoilMoistureScreen::setCalibrationInfo(uint16_t dryMv, uint16_t wetMv) {
  if (_dryMv != dryMv || _wetMv != wetMv) {
    _dryMv = dryMv;
    _wetMv = wetMv;
    _infoDirty = true;
    if (_active) draw();
  }
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
  _armed = Armed::None;
  _feedback[0] = '\0';
  draw();
}

void SoilMoistureScreen::onExit() {
  _active = false;
  _armed = Armed::None;
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
  uint32_t now = millis();

  // Disarm a button that was tapped once and then left alone.
  if (_armed != Armed::None && now - _armedAtMs > kArmTimeoutMs) {
    _armed = Armed::None;
    _buttonsDirty = true;
  }
  // Drop the result banner back to the info line.
  if (_feedback[0] != '\0' && now - _feedbackAtMs > kFeedbackTimeoutMs) {
    _feedback[0] = '\0';
    _infoDirty = true;
  }

  if (_infoDirty || _buttonsDirty || now - _lastDrawMs >= kDrawIntervalMs) {
    draw();
  }
}

void SoilMoistureScreen::onVerticalTouch(int32_t x, int32_t y) {
  if (y < kBtnY1 || y > kBtnY2) return;
  if (x >= kDryBtnX && x < kDryBtnX + kBtnW) {
    _onButtonTap(Armed::Dry);
  } else if (x >= kWetBtnX && x < kWetBtnX + kBtnW) {
    _onButtonTap(Armed::Wet);
  }
}

void SoilMoistureScreen::onBtnB() {
  // Physical middle button confirms an armed capture (same as a second tap).
  if (_armed != Armed::None) _onButtonTap(_armed);
}

void SoilMoistureScreen::_onButtonTap(Armed which) {
  uint32_t now = millis();

  // First tap arms, second tap on the same button (within the window) fires.
  if (_armed != which) {
    _armed = which;
    _armedAtMs = now;
    _buttonsDirty = true;
    draw();
    return;
  }
  _armed = Armed::None;
  _buttonsDirty = true;

  const bool wet = (which == Armed::Wet);
  bool ok = _calibrate ? _calibrate(wet) : false;
  if (ok) {
    snprintf(_feedback, sizeof(_feedback), "%s point saved: %u mV",
             wet ? "WET" : "DRY", static_cast<unsigned>(_milliVolts));
  } else {
    snprintf(_feedback, sizeof(_feedback), "%s point rejected", wet ? "WET" : "DRY");
  }
  _feedbackOk = ok;
  _feedbackAtMs = now;
  _infoDirty = true;
  draw();
}

template<typename GFX>
void SoilMoistureScreen::_renderInfo(GFX& gfx) {
  gfx.fillRect(kContentX, kInfoY, kContentW, kInfoH, TFT_BLACK);
  gfx.setFont(&lgfx::fonts::DejaVu9);
  gfx.setTextSize(1);
  gfx.setTextDatum(textdatum_t::middle_center);

  char line[64];
  if (_feedback[0] != '\0') {
    gfx.setTextColor(_feedbackOk ? TFT_GREEN : TFT_RED, TFT_BLACK);
    strlcpy(line, _feedback, sizeof(line));
  } else {
    gfx.setTextColor(0xC618U /* light grey */, TFT_BLACK);
    if (_hasMilliVolts) {
      snprintf(line, sizeof(line), "%u mV   |   dry %u  /  wet %u",
               static_cast<unsigned>(_milliVolts),
               static_cast<unsigned>(_dryMv), static_cast<unsigned>(_wetMv));
    } else {
      snprintf(line, sizeof(line), "--- mV   |   dry %u  /  wet %u",
               static_cast<unsigned>(_dryMv), static_cast<unsigned>(_wetMv));
    }
  }
  gfx.drawString(line, kContentX + kContentW / 2, kInfoY + kInfoH / 2);
  _infoDirty = false;
}

template<typename GFX>
void SoilMoistureScreen::_renderButtons(GFX& gfx) {
  // Same visual language as RgbLightScreen: dark idle, accent when armed.
  struct Btn { int16_t x; Armed id; const char* idle; uint16_t accentBg; uint16_t accentBdr; };
  const Btn buttons[] = {
    { kDryBtnX, Armed::Dry, "SET DRY", 0x8200U /* dark orange */, 0xFD20U /* orange */ },
    { kWetBtnX, Armed::Wet, "SET WET", 0x0318U /* dark cyan */,   0x07FFU /* cyan */ },
  };

  gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
  gfx.setTextSize(1);
  gfx.setTextDatum(textdatum_t::middle_center);
  for (const Btn& b : buttons) {
    bool armed = (_armed == b.id);
    gfx.fillRoundRect(b.x, kBtnY1, kBtnW, kBtnY2 - kBtnY1, 8, armed ? b.accentBg : 0x2104U);
    gfx.drawRoundRect(b.x, kBtnY1, kBtnW, kBtnY2 - kBtnY1, 8, armed ? b.accentBdr : 0x4208U);
    gfx.setTextColor(TFT_WHITE, TFT_BLACK);
    gfx.drawString(armed ? "TAP AGAIN" : b.idle, b.x + kBtnW / 2, (kBtnY1 + kBtnY2) / 2);
  }
  _buttonsDirty = false;
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

  // ---- Info line + calibration buttons ----
  if (didFullClear || _infoDirty)    _renderInfo(gfx);
  if (didFullClear || _buttonsDirty) _renderButtons(gfx);

  // ---- Carousel dots (only on full clear) ----
  if (didFullClear) {
    ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
  }

  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(nullptr);
}

template void SoilMoistureScreen::_render<M5GFX>(M5GFX&, bool);
template void SoilMoistureScreen::_render<LGFX_Sprite>(LGFX_Sprite&, bool);
