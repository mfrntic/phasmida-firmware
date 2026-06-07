#include <sensors/Env3Probe.h>
#include <ui/EnvSensorScreen.h>

void Env3Probe::feedScreens(const SensorReading& r) {
  if (_screen) {
    _screen->notifyNewReadings(r.temperatureC, r.humidityPct, r.pressurePa);
  }
}
