#pragma once

#include <Arduino.h>
#include <cstring>
#include <M5GFX.h>

namespace ui {

/// Reusable measurement card widget.
///
/// A self-contained drawable rectangle showing:
///   - small label  (top-left)
///   - large value  with a softer inline unit
///
/// Configurable: position, size, label, unit, value color, precision, optional border.
/// Tracks a "dirty" flag so screens can skip redraw when value hasn't changed.
///
/// The card always clears its own rectangle on render — caller doesn't need to
/// pre-clear the area.
class MeasurementCard {
public:
  MeasurementCard(int16_t x, int16_t y, int16_t w, int16_t h)
    : _x(x), _y(y), _w(w), _h(h) {}

  // ---- geometry ----
  void setBounds(int16_t x, int16_t y, int16_t w, int16_t h) {
    _x = x; _y = y; _w = w; _h = h;
    _dirty = true;
  }
  int16_t x() const { return _x; }
  int16_t y() const { return _y; }
  int16_t w() const { return _w; }
  int16_t h() const { return _h; }

  // Distribute N cards proportionally inside a vertical area [y, y+totalH],
  // each spanning the full width `w` starting at `x`. Total available height
  // is split equally between cards minus (n-1)*gap pixels for spacing.
  static void layoutVertical(MeasurementCard* cards[], size_t n,
                             int16_t x, int16_t y, int16_t w, int16_t totalH,
                             int16_t gap = 4) {
    if (!cards || n == 0 || totalH <= 0) return;
    int16_t totalGap = (n > 1) ? static_cast<int16_t>(gap * (n - 1)) : 0;
    int16_t cardH = static_cast<int16_t>((totalH - totalGap) / n);
    if (cardH < 1) cardH = 1;
    int16_t cy = y;
    for (size_t i = 0; i < n; ++i) {
      if (cards[i]) cards[i]->setBounds(x, cy, w, cardH);
      cy = static_cast<int16_t>(cy + cardH + gap);
    }
  }

  // ---- configuration ----
  void setLabel(const char* label)        { _label = label; _dirty = true; }
  void setUnit(const char* unit)          { _unit  = unit;  _dirty = true; }
  void setValueColor(uint16_t c)          { _valueColor = c; _dirty = true; }
  void setLabelColor(uint16_t c)          { _labelColor = c; _dirty = true; }
  void setBorderColor(uint16_t c)         { _borderColor = c; _hasBorder = true; _dirty = true; }
  void setNoBorder()                      { _hasBorder = false; _dirty = true; }
  void setBgColor(uint16_t c)             { _bgColor = c; _dirty = true; }
  void setPrecision(uint8_t p)            { _precision = p; _dirty = true; }
  void setLabelScale(float s)             { _labelScale = s; _dirty = true; }
  void setValueScale(float s)             { _valueScale = s; _dirty = true; }

  // ---- value update ----
  void setValue(float v) {
    // By default, refresh whenever the displayed (rounded-to-precision)
    // value would actually differ. This keeps the screen in sync with the
    // freshest sample (and with telemetry payloads). A caller can override
    // this via setChangeThreshold().
    float thresh = (_changeThreshold >= 0.0f) ? _changeThreshold : _autoThreshold();
    if (!_hasValue || fabsf(v - _value) >= thresh) {
      _value = v;
      _hasValue = true;
      _dirty = true;
    } else {
      _value = v;
    }
  }
  // Pass a negative value to switch back to auto (precision-derived) threshold.
  void setChangeThreshold(float t)        { _changeThreshold = t; }
  void clearValue()                       { _hasValue = false; _dirty = true; }

  bool isDirty() const                    { return _dirty; }
  void markRendered()                     { _dirty = false; }

  // ---- rendering ----
  template<typename GFX>
  void render(GFX& gfx) {
    // Clear card background
    if (_hasBorder) {
      gfx.fillRoundRect(_x, _y, _w, _h, 6, _bgColor);
      gfx.drawRoundRect(_x, _y, _w, _h, 6, _borderColor);
    } else {
      gfx.fillRect(_x, _y, _w, _h, _bgColor);
    }

    // Vrijednost — horizontalno centrirano, veliki bold font (crta se PRVO
    // da pozadinski fill ne bi prekrio labelu koja dolazi nakon)
    int16_t valY = _y + _h / 2 + 13;
    gfx.setFont(&lgfx::fonts::FreeSansBold24pt7b);
    gfx.setTextSize(_valueScale);
    gfx.setTextDatum(textdatum_t::middle_center);
    if (_hasValue) {
      gfx.setTextColor(_valueColor);
      String valueStr = String(_value, (int)_precision);
      gfx.drawString(valueStr, _x + _w / 2, valY);
    } else {
      gfx.setTextColor(_labelColor);
      gfx.drawString("--", _x + _w / 2, valY);
    }

    // Label — lijevo poravnato, DejaVu9 nativna visina ~9px, scale 1.0
    gfx.setFont(&lgfx::fonts::DejaVu9);
    gfx.setTextSize(1.0f);
    gfx.setTextColor(_labelColor);
    gfx.setTextDatum(textdatum_t::top_left);
    if (_label) gfx.drawString(_label, _x + 8, _y + 2);

    // Jedinica — desno poravnato, FreeSans9pt za Unicode podrku (Ω i sl.)
    if (_unit && _unit[0] != '\0') {
      gfx.setFont(&lgfx::fonts::FreeSans9pt7b);
      gfx.setTextSize(1.0f);
      gfx.setTextColor(_labelColor);
      _drawUnitRight(gfx, _x + _w - 8, _y);
    }

    // Reset state
    gfx.setTextDatum(textdatum_t::top_left);
    gfx.setFont(nullptr);
    gfx.setTextSize(1);

    _dirty = false;
  }

  // Crta jedinicu desno-poravnatu: rightX je desni rub, topY je vrh teksta.
  template<typename GFX>
  void _drawUnitRight(GFX& gfx, int16_t rightX, int16_t topY) {
    gfx.setTextDatum(textdatum_t::top_right);

    if (std::strcmp(_unit, "\xB0" "C") == 0) {
      // "C" desno poravnato, kružić za stupanj lijevo od njega
      gfx.drawString("C", rightX, topY);
      int16_t cW = gfx.textWidth("C");
      int16_t circleX = rightX - cW - 4;
      int16_t circleY = topY + 1;
      gfx.drawCircle(circleX, circleY, 2, _labelColor);
      return;
    }

    gfx.drawString(_unit, rightX, topY);
  }

private:
  int16_t  _x, _y, _w, _h;
  const char* _label = "";
  const char* _unit  = "";
  uint16_t _valueColor      = TFT_WHITE;
  uint16_t _labelColor      = 0x8410U;   // mid grey
  uint16_t _borderColor     = 0x4208U;   // dim grey
  uint16_t _bgColor         = 0x1082U;   // very dark blue-grey  (RGB565 ~#101010)
  bool     _hasBorder       = true;
  uint8_t  _precision       = 2;
  float    _labelScale      = 0.7f;
  float    _valueScale      = 0.8f;
  float    _value           = 0.0f;
  bool     _hasValue        = false;
  // Negative => use auto threshold derived from _precision (half of last
  // displayed decimal). Set explicitly to override.
  float    _changeThreshold = -1.0f;
  bool     _dirty           = true;

  // 0.5 * 10^-_precision — smallest delta that could change the displayed
  // rounded value. Cap at p<=6 to avoid float precision games.
  float _autoThreshold() const {
    static const float kStep[7] = {
      0.5f, 0.05f, 0.005f, 0.0005f, 0.00005f, 0.000005f, 0.0000005f
    };
    uint8_t p = (_precision <= 6) ? _precision : 6;
    return kStep[p];
  }
};

} // namespace ui
