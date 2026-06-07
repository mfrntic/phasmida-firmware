#include <sensors/EnvProProbe.h>

void     EnvProProbe::feedScreens(const SensorReading& /*r*/) {}
size_t   EnvProProbe::screenCount() const { return 0; }
IScreen* EnvProProbe::screen(size_t /*idx*/) const { return nullptr; }
