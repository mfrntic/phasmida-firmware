#include <sensors/SoilMoistureProbe.h>

void     SoilMoistureProbe::feedScreens(const SensorReading& /*r*/) {}
size_t   SoilMoistureProbe::screenCount() const { return 0; }
IScreen* SoilMoistureProbe::screen(size_t /*idx*/) const { return nullptr; }
