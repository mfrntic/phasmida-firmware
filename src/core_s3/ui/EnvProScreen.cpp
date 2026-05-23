#include "ui/EnvProScreen.h"
#include "ui/UICommon.h"
#include <M5Unified.h>

EnvProScreen::EnvProScreen() {
  _configureCards();
}

void EnvProScreen::_configureCards() {
  // Proporcionalno popuni isti vertikalni prostor (y=24..214, h=190) — s 2
  // kartice svaka dobiva ~93 px visine umjesto fiksnih 62 px.
  ui::MeasurementCard* cards[] = { &_cardGas, &_cardIaq };
  ui::MeasurementCard::layoutVertical(cards, 2, /*x*/30, /*y*/24, /*w*/282, /*h*/190, /*gap*/4);

  _cardGas.setLabel(_gasLabelBuf);
  _cardGas.setUnit("kOhm");
  _cardGas.setValueColor(TFT_ORANGE);
  _cardGas.setPrecision(1);
  _cardGas.setNoBorder();

  _cardIaq.setLabel(_iaqLabelBuf);
  _cardIaq.setUnit("idx");
  _cardIaq.setValueColor(TFT_GREEN);
  _cardIaq.setPrecision(0);
  _cardIaq.setNoBorder();
}

int EnvProScreen::_computeGasTrend() const {
  if (_gasHistCount < 4) return 0;
  // Compare mean of newest half vs oldest half.
  uint8_t half = _gasHistCount / 2;
  float oldSum = 0.0f, newSum = 0.0f;
  for (uint8_t i = 0; i < half; ++i) {
    uint8_t oldIdx = (_gasHistHead + kGasHistory - _gasHistCount + i) % kGasHistory;
    uint8_t newIdx = (_gasHistHead + kGasHistory - half + i) % kGasHistory;
    oldSum += _gasHistKohm[oldIdx];
    newSum += _gasHistKohm[newIdx];
  }
  float oldAvg = oldSum / half;
  float newAvg = newSum / half;
  if (oldAvg <= 0.0f) return 0;
  float relDelta = (newAvg - oldAvg) / oldAvg;
  if (relDelta >  0.02f) return  1;  // > +2 %
  if (relDelta < -0.02f) return -1;  // < -2 %
  return 0;
}

void EnvProScreen::_updateCardsFromData() {
  // ---- Gas card ----
  if (_data.hasGas) {
    float gasKohm = _data.gasResistanceOhm / 1000.0f;

    _gasHistKohm[_gasHistHead] = gasKohm;
    _gasHistHead = (_gasHistHead + 1) % kGasHistory;
    if (_gasHistCount < kGasHistory) ++_gasHistCount;

    int trend = _computeGasTrend();
    const char* arrow = (trend > 0) ? " ^" : (trend < 0) ? " v" : " =";
    snprintf(_gasLabelBuf, sizeof(_gasLabelBuf), "GAS%s", arrow);

    _cardGas.setLabel(_gasLabelBuf);  // re-bind (safe; same buffer)
    _cardGas.setValue(gasKohm);
  } else {
    _cardGas.clearValue();
  }

  // ---- IAQ card ----
  if (_data.hasIaq) {
    if (_data.iaqAccuracy == 0) {
      // Algorithm not yet calibrated — value is meaningless, hide it.
      snprintf(_iaqLabelBuf, sizeof(_iaqLabelBuf), "IAQ  CALIBRATING");
      _cardIaq.setLabel(_iaqLabelBuf);
      _cardIaq.clearValue();
    } else {
      snprintf(_iaqLabelBuf, sizeof(_iaqLabelBuf), "IAQ  acc:%u",
               static_cast<unsigned>(_data.iaqAccuracy));
      _cardIaq.setLabel(_iaqLabelBuf);
      _cardIaq.setValue(_data.iaq);
    }
  } else {
    _cardIaq.clearValue();
  }
}

void EnvProScreen::notifyNewReadings(const SensorReading& data) {
  _data = data;
  _updateCardsFromData();
  if (_active) {
    draw();
  }
}

void EnvProScreen::setNavInfo(int myIdx, int total) {
  if (_myIndex != myIdx || _totalScreens != total) {
    _needsFullClear = true;
  }
  _myIndex = myIdx;
  _totalScreens = total;
}

void EnvProScreen::onExit() {
  _active = false;
}

void EnvProScreen::onEnter() {
  _active = true;
  _needsFullClear = true;
  draw();
}

void EnvProScreen::draw() {
  // Always render the layout; cards show "--" until BSEC produces output.
  _render(M5.Display, false);
  _lastDrawMs = millis();
}

void EnvProScreen::drawIntoSprite(LGFX_Sprite& sp) {
  bool savedNfc = _needsFullClear;
  _needsFullClear = true;
  _render(sp, true);
  _needsFullClear = savedNfc;
}

void EnvProScreen::onUpdate() {
  if (!_active) {
    return;
  }
  if (millis() - _lastDrawMs >= kDrawIntervalMs) {
    draw();
  }
}

void EnvProScreen::onBtnB() {
}

template <typename GFX>
void EnvProScreen::_render(GFX& gfx, bool /*forceFull*/) {
  bool didFullClear = _needsFullClear;

  if (didFullClear) {
    gfx.fillScreen(TFT_BLACK);
    _needsFullClear = false;

    gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
    gfx.setTextSize(1);
    gfx.setTextColor(0xC618U, TFT_BLACK);
    gfx.setTextDatum(textdatum_t::middle_center);

    constexpr int16_t kRailCenterX = 12;
    constexpr int16_t kRailTopY = 24;
    constexpr int16_t kRailBottomY = 214;
    const int16_t kCharStep = gfx.fontHeight() + 2;
    const int16_t kWordGap = kCharStep / 2;

    int16_t totalH = 0;
    for (const char* p = kProbeName; *p; ++p) {
      totalH += (*p == ' ') ? kWordGap : kCharStep;
    }
    if (totalH > 0) {
      totalH -= kCharStep;
    }

    const int16_t railH = (kRailBottomY - kRailTopY);
    int16_t titleY = kRailTopY + ((railH - totalH) / 2);

    for (const char* p = kProbeName; *p; ++p) {
      if (*p == ' ') {
        titleY += kWordGap;
        continue;
      }
      char ch[2] = {*p, '\0'};
      gfx.drawString(ch, kRailCenterX, titleY);
      titleY += kCharStep;
    }

    gfx.setTextDatum(textdatum_t::top_left);
    gfx.drawFastVLine(24, 20, 196, 0x4208U);
  }

  if (didFullClear || _cardGas.isDirty()) _cardGas.render(gfx);
  if (didFullClear || _cardIaq.isDirty()) _cardIaq.render(gfx);

  if (didFullClear) {
    ui::drawCarouselDots(gfx, _totalScreens, _myIndex);
  }

  gfx.setTextDatum(textdatum_t::top_left);
  gfx.setFont(nullptr);
}

template void EnvProScreen::_render<M5GFX>(M5GFX&, bool);
template void EnvProScreen::_render<LGFX_Sprite>(LGFX_Sprite&, bool);
