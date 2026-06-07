#include <sensors/SoilMoistureProbe.h>
#include <ui/SoilMoistureScreen.h>

void SoilMoistureProbe::feedScreens(const SensorReading& r) {
  if (_screen && r.hasSoilMoisture) {
    _screen->notifyNewReadings(r.soilMoisturePct);
  }
}

size_t SoilMoistureProbe::screenCount() const {
  return _screen ? 1 : 0;
}

IScreen* SoilMoistureProbe::screen(size_t idx) const {
  return (idx == 0 && _screen) ? _screen : nullptr;
}
