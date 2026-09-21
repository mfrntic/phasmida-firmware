#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include <functional>
#include "IScreen.h"
#include "MeasurementCard.h"

// Soil Moisture display screen: moisture percentage card, the live AOUT level
// with the active calibration pair, and two on-device calibration buttons
// (SET DRY / SET WET). A button must be tapped twice within a short window
// so a stray touch cannot overwrite a calibration point.
class SoilMoistureScreen : public IScreen {
 public:
  // Capture the live level as the wet (true) or dry (false) point.
  // Returns false if the capture was rejected (probe absent / invalid pair).
  using CalibrateFn = std::function<bool(bool wet)>;

  SoilMoistureScreen();
  ~SoilMoistureScreen() override = default;

  void notifyNewReadings(float moisturePct, uint16_t milliVolts);
  void setCalibrationInfo(uint16_t dryMv, uint16_t wetMv);
  void setCalibrateCallback(CalibrateFn fn) { _calibrate = std::move(fn); }

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void onExit() override;
  void draw() override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onUpdate() override;
  void onVerticalTouch(int32_t x, int32_t y) override;
  void onBtnB() override;

 private:
  enum class Armed : uint8_t { None, Dry, Wet };

  template<typename GFX>
  void _render(GFX& gfx, bool forceFull);
  template<typename GFX>
  void _renderInfo(GFX& gfx);
  template<typename GFX>
  void _renderButtons(GFX& gfx);

  void _onButtonTap(Armed which);

  bool _active = false;
  uint32_t _lastDrawMs = 0;
  static constexpr uint32_t kDrawIntervalMs = 2000;
  bool _needsFullClear = true;
  bool _infoDirty = true;
  bool _buttonsDirty = true;

  int _myIndex = 0;
  int _totalScreens = 0;

  // Layout (320x240): card on top, info line, then two side-by-side buttons.
  static constexpr int16_t kContentX   = 30;
  static constexpr int16_t kContentW   = 282;
  static constexpr int16_t kInfoY      = 146;
  static constexpr int16_t kInfoH      = 18;
  static constexpr int16_t kBtnY1      = 170;
  static constexpr int16_t kBtnY2      = 210;
  static constexpr int16_t kBtnGap     = 10;
  static constexpr int16_t kBtnW       = (kContentW - kBtnGap) / 2;
  static constexpr int16_t kDryBtnX    = kContentX;
  static constexpr int16_t kWetBtnX    = kContentX + kBtnW + kBtnGap;
  static constexpr uint32_t kArmTimeoutMs      = 3000;
  static constexpr uint32_t kFeedbackTimeoutMs = 2500;

  ui::MeasurementCard _cardMoisture{kContentX, 24, kContentW, 116};

  uint16_t _milliVolts = 0;
  bool     _hasMilliVolts = false;
  uint16_t _dryMv = 0;
  uint16_t _wetMv = 0;

  Armed    _armed = Armed::None;
  uint32_t _armedAtMs = 0;
  // Short result banner in place of the info line after a capture attempt.
  char     _feedback[40] = {0};
  bool     _feedbackOk = true;
  uint32_t _feedbackAtMs = 0;

  CalibrateFn _calibrate;
};
