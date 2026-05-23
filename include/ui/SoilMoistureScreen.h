#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include "IScreen.h"
#include "MeasurementCard.h"

// Soil Moisture display screen showing moisture percentage.
// Simple single-card layout for soil moisture data from Unit Earth.
class SoilMoistureScreen : public IScreen {
 public:
  SoilMoistureScreen();
  ~SoilMoistureScreen() override = default;

  void notifyNewReadings(float moisturePct);

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void onExit() override;
  void draw() override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onUpdate() override;
  void onBtnB() override;

 private:
  template<typename GFX>
  void _render(GFX& gfx, bool forceFull);

  bool _active = false;
  uint32_t _lastDrawMs = 0;
  static constexpr uint32_t kDrawIntervalMs = 2000;
  bool _needsFullClear = true;

  int _myIndex = 0;
  int _totalScreens = 0;

  ui::MeasurementCard _cardMoisture{30, 24, 282, 190};
};
