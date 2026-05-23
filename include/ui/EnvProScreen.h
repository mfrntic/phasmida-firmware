#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include <sensors/SensorReading.h>
#include "IScreen.h"
#include "MeasurementCard.h"

// "PRO" extras shown alongside the basic ENV screen.
// Currently: gas resistance (with short-term trend) and IAQ (with calibration accuracy).
// All sensor-agnostic — driven by notifyNewReadings(SensorReading).
class EnvProScreen : public IScreen {
public:
  EnvProScreen();

  void notifyNewReadings(const SensorReading& data);

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void onExit() override;
  void draw() override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onUpdate() override;
  void onBtnB() override;

private:
  template <typename GFX>
  void _render(GFX& gfx, bool forceFull);

  void _configureCards();
  void _updateCardsFromData();

  // Gas-resistance trend over the last N samples.
  // Returns -1 (falling), 0 (stable), +1 (rising).
  int _computeGasTrend() const;

  SensorReading _data{};
  bool _active = false;
  bool _needsFullClear = true;
  uint32_t _lastDrawMs = 0;
  static constexpr uint32_t kDrawIntervalMs = 2000;

  int _myIndex = 0;
  int _totalScreens = 0;

  // Ring buffer for short-term gas trend (kΩ).
  static constexpr uint8_t kGasHistory = 10;
  float    _gasHistKohm[kGasHistory] = {0};
  uint8_t  _gasHistCount = 0;
  uint8_t  _gasHistHead  = 0;

  // Stable backing storage for dynamic card labels (card stores raw pointers).
  char _gasLabelBuf[24] = "GAS";
  char _iaqLabelBuf[24] = "IAQ";

  // Two wide stacked cards (matches EnvSensorScreen visual style).
  ui::MeasurementCard _cardGas { 30,  56, 282, 62 };
  ui::MeasurementCard _cardIaq { 30, 130, 282, 62 };

  static constexpr const char* kProbeName = "ENV PRO";
};
