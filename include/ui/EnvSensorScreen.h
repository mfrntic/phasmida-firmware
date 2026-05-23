#pragma once

#include <Arduino.h>
#include <M5GFX.h>
#include "IScreen.h"
#include "MeasurementCard.h"

// Generic basic environmental screen: temperature, humidity, pressure.
// Sensor-agnostic — driven by notifyNewReadings(). Currently fed by
// ENV PRO (BME688) data, but works with any probe that exposes T/H/P.
class EnvSensorScreen : public IScreen {
public:
  explicit EnvSensorScreen(const char* probeTitle = "ENV");

  void notifyNewReadings(float tempC, float humPct, float pressPa);

  void setNavInfo(int myIdx, int total) override;
  void onEnter() override;
  void onExit()  override;
  void draw()    override;
  void drawIntoSprite(LGFX_Sprite& sp) override;
  void onUpdate() override;
  void onBtnB()   override;

private:
  template<typename GFX>
  void _render(GFX& gfx, bool forceFull);

  void _configureCards();

  bool     _active   = false;

  uint32_t _lastDrawMs   = 0;
  static constexpr uint32_t kDrawIntervalMs = 2000;

  bool     _needsFullClear = true;

  int      _myIndex      = 0;
  int      _totalScreens = 0;

  ui::MeasurementCard _cardTemp  { 30,  24, 282, 62 };
  ui::MeasurementCard _cardHum   { 30,  88, 282, 62 };
  ui::MeasurementCard _cardPress { 30, 152, 282, 62 };

  const char* _probeTitle = "ENV";
};
