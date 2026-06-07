#include <sensors/EnvProProbe.h>
#include <ui/EnvProScreen.h>
#include <ui/EnvSensorScreen.h>

size_t EnvProProbe::screenCount() const {
  size_t n = 0;
  if (_basic) ++n;
  if (_pro)   ++n;
  return n;
}

IScreen* EnvProProbe::screen(size_t idx) const {
  if (_basic && idx == 0) return _basic;
  if (_pro && idx == (_basic ? 1u : 0u)) return _pro;
  return nullptr;
}

void EnvProProbe::feedScreens(const SensorReading& r) {
  if (_basic) {
    _basic->notifyNewReadings(r.temperatureC, r.humidityPct, r.pressurePa);
  }
  if (_pro) {
    _pro->notifyNewReadings(r);
  }
}
