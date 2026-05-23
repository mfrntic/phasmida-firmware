#pragma once

#include <bsec2.h>

#include <sensors/ISensorProbe.h>
#include <ui/EnvProScreen.h>
#include <ui/EnvSensorScreen.h>

// EnvProProbe
// -----------
// M5Stack ENV PRO unit: BME688 driven by Bosch BSEC2 (IAQ algorithm).
// Provides temperature, humidity, pressure, gas resistance, and IAQ index.
// I2C address 0x77.
//
// BSEC2 uses a C-style callback that has no userdata pointer, so this
// class is effectively a singleton (one instance can be registered at a
// time). Constructing a second instance overwrites the bridge.
//
// Owns two screens: a basic T/H/P card and a "PRO extras" card (gas + IAQ).
class EnvProProbe : public ISensorProbe {
 public:
  EnvProProbe(EnvSensorScreen* basicScreen, EnvProScreen* proScreen);

  const char* name() const override { return "ENV PRO"; }
  const char* telemetryType() const override { return "env-pro"; }

  bool detect() override;
  bool init() override;
  void service() override;
  void shutdown() override;
  bool sample(SensorReading& out) override;

  size_t   screenCount() const override;
  IScreen* screen(size_t idx) const override;
  void     feedScreens(const SensorReading& r) override;

 private:
  static EnvProProbe* s_instance;
  static void         _bsecBridge(const bme68xData data,
                                  const bsecOutputs outputs,
                                  const Bsec2 bsec);
  void                _onBsecOutputs(const bsecOutputs& outputs);

  EnvSensorScreen* _basic;
  EnvProScreen*    _pro;
  Bsec2            _bsec;

  SensorReading _latest{};
  bool          _ready        = false;
  uint32_t      _lastSampleMs = 0;
  bool          _hasTemp      = false;
  bool          _hasHum       = false;
  bool          _hasPress     = false;
};
