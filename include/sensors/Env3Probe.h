#pragma once

#include <M5UnitENV.h>

#include <sensors/ISensorProbe.h>
#include <ui/EnvSensorScreen.h>

// Env3Probe
// ---------
// M5Stack ENV 3 unit: SHT31 (T/H @ 0x44) + QMP6988 (P @ 0x70).
// Pure I2C, blocking reads, no callback machinery. Owns one basic env screen.
class Env3Probe : public ISensorProbe {
 public:
  explicit Env3Probe(EnvSensorScreen* basicScreen) : _screen(basicScreen) {}

  const char* name() const override { return "ENV 3"; }
  const char* telemetryType() const override { return "env-iii"; }

  bool detect() override;
  bool init() override;
  bool sample(SensorReading& out) override;

  size_t   screenCount() const override { return _screen ? 1 : 0; }
  IScreen* screen(size_t idx) const override { return idx == 0 ? _screen : nullptr; }
  void     feedScreens(const SensorReading& r) override;

 private:
  EnvSensorScreen* _screen;
  SHT3X            _sht3x;
  QMP6988          _qmp;
};
