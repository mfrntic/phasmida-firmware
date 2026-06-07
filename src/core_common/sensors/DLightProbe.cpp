#include <sensors/DLightProbe.h>

#include <Wire.h>

#include <app_config.h>
#include <ui/DLightScreen.h>

namespace {
bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
}  // namespace

DLightProbe::DLightProbe(DLightScreen* screen)
    : _screen(screen),
      _sensor(AppConfig::kDLightI2cAddr),
      _i2cAddr(AppConfig::kDLightI2cAddr),
      _isInitialized(false),
      _lastLux(0) {}

const char* DLightProbe::name() const {
  return "LIGHT";
}

const char* DLightProbe::telemetryType() const {
  return "light";
}

bool DLightProbe::detect() {
  Wire.begin(AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  return i2cPing(_i2cAddr);
}

bool DLightProbe::init() {
  Wire.begin(AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  if (!i2cPing(_i2cAddr)) {
    _isInitialized = false;
    return false;
  }

  _sensor.begin(&Wire, AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  _sensor.setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
  _isInitialized = true;
  return true;
}

bool DLightProbe::sample(SensorReading& out) {
  if (!_isInitialized) {
    return false;
  }

  // Fast bus check prevents stale reads after unplug and helps sample-level
  // failure path until presence polling removes the probe.
  if (!i2cPing(_i2cAddr)) {
    return false;
  }

  uint16_t lux = _sensor.getLUX();
  _lastLux = lux;

  out = SensorReading{};
  out.hasLux = true;
  out.lux = static_cast<float>(lux);
  return true;
}

void DLightProbe::feedScreens(const SensorReading& r) {
  if (_screen && r.hasLux) {
    _screen->notifyNewReadings(r.lux);
  }
}

size_t DLightProbe::screenCount() const {
  return _screen ? 1 : 0;
}

IScreen* DLightProbe::screen(size_t idx) const {
  return (idx == 0 && _screen) ? _screen : nullptr;
}

void DLightProbe::shutdown() {
  _isInitialized = false;
}
