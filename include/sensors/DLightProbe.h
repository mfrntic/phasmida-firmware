#pragma once

#include <M5_DLight.h>

#include <sensors/ISensorProbe.h>

class DLightScreen;

class DLightProbe : public ISensorProbe {
 public:
  explicit DLightProbe(DLightScreen* screen = nullptr);
  ~DLightProbe() override = default;

  const char* name() const override;
  const char* telemetryType() const override;

  bool detect() override;
  bool init() override;
  bool sample(SensorReading& out) override;
  void feedScreens(const SensorReading& r) override;

  size_t screenCount() const override;
  IScreen* screen(size_t idx) const override;

  void service() override {}
  void shutdown() override;

 private:
  DLightScreen* _screen;
  M5_DLight     _sensor;
  uint8_t       _i2cAddr;
  bool          _isInitialized;
  uint16_t      _lastLux;
};
