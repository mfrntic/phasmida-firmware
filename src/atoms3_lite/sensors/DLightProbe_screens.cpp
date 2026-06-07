#include <sensors/DLightProbe.h>

void   DLightProbe::feedScreens(const SensorReading& /*r*/) {}
size_t DLightProbe::screenCount() const { return 0; }
IScreen* DLightProbe::screen(size_t /*idx*/) const { return nullptr; }
