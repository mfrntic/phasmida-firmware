#include <Arduino.h>

#include <sensors/Env3Probe.h>
#include <sensors/EnvProProbe.h>
#include <sensors/SoilMoistureProbe.h>
#include <sensors/DLightProbe.h>

#include "../core_common/app_core.h"

// Sensor probes with nullptr screen pointers (headless — no display).
static Env3Probe         g_env3Probe(nullptr);
static EnvProProbe       g_envProProbe(nullptr, nullptr);
static SoilMoistureProbe g_soilMoistureProbe(nullptr);
static DLightProbe       g_dlightProbe(nullptr);

void setup() {
  appRegisterProbe(&g_env3Probe);
  appRegisterProbe(&g_envProProbe);
  appRegisterProbe(&g_soilMoistureProbe);
  appRegisterProbe(&g_dlightProbe);

  // Empty hooks — all UI callbacks are no-ops for headless operation.
  appBegin(AppUiHooks{});
}

void loop() {
  appUpdate();
}
