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

 private:
  SoilMoistureScreen* _screen;
  uint8_t  _analogPin;
  uint8_t  _digitalPin;
  uint16_t _lastRawValue;
  bool     _isInitialized;
};
