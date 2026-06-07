#include <sensors/Env3Probe.h>

#include <Wire.h>

#include <app_config.h>
#include <ui/EnvSensorScreen.h>

namespace {
bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
}  // namespace

bool Env3Probe::detect() {
  // Wire.begin is idempotent on ESP32; safe to call from each probe.
  Wire.begin(AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  return i2cPing(SHT3X_I2C_ADDR) && i2cPing(QMP6988_SLAVE_ADDRESS_L);
}

bool Env3Probe::init() {
  bool qmpOk = _qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  bool shtOk = _sht3x.begin(&Wire, SHT3X_I2C_ADDR,        AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  return qmpOk && shtOk;
}

bool Env3Probe::sample(SensorReading& out) {
  if (!_sht3x.update() || !_qmp.update()) {
    return false;
  }
  if (isnan(_sht3x.cTemp) || isnan(_sht3x.humidity) || isnan(_qmp.pressure)) {
    return false;
  }
  out               = SensorReading{};
  out.hasTemperature = true; out.temperatureC = _sht3x.cTemp;
  out.hasHumidity    = true; out.humidityPct  = _sht3x.humidity;
  out.hasPressure    = true; out.pressurePa   = _qmp.pressure;
  return true;
}

void Env3Probe::feedScreens(const SensorReading& r) {
  if (_screen) {
    _screen->notifyNewReadings(r.temperatureC, r.humidityPct, r.pressurePa);
  }
}
