#pragma once

#include <sensors/ISensorProbe.h>

// Forward declaration
class SoilMoistureScreen;

class SoilMoistureProbe : public ISensorProbe {
 public:
  // Optionally pass a screen to display readings; if nullptr, no UI updates.
  explicit SoilMoistureProbe(SoilMoistureScreen* screen = nullptr);
  ~SoilMoistureProbe() override = default;

  // ISensorProbe implementation
  const char* name() const override;
  const char* telemetryType() const override;

  bool detect() override;
  bool init() override;
  bool sample(SensorReading& out) override;
  void feedScreens(const SensorReading& r) override;

  size_t screenCount() const override;
  IScreen* screen(size_t idx) const override;

  void service() override {}
  void shutdown() override {}

  // Per-pot calibration: AOUT level (mV) mapped to 0 % and to 100 %.
  // Defaults come from app_config.h; runtime values are loaded from NVS.
  void     setCalibration(uint16_t dryMv, uint16_t wetMv);
  uint16_t dryMv() const { return _dryMv; }
  uint16_t wetMv() const { return _wetMv; }

  // Fresh eFuse-calibrated AOUT reading, for capturing a calibration point
  // on demand (independent of the sampling cadence).
  uint16_t readMilliVolts() const;

 private:
  SoilMoistureScreen* _screen;
  uint8_t  _analogPin;
  uint8_t  _digitalPin;
  uint16_t _lastRawValue;
  bool     _isInitialized;
  uint16_t _dryMv;
  uint16_t _wetMv;
};
