#include <sensors/DLightProbe.h>
#include <ui/DLightScreen.h>

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
