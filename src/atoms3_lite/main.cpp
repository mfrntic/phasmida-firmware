#include <Arduino.h>
#include <M5Unified.h>

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
static bool              g_rgbVerifyPending = false;

void setup() {
  // Bring up serial as early as possible so boot failures are visible.
  Serial.begin(115200);
  delay(120);
  Serial.println("[atoms3_lite] setup: early boot");

  appRegisterProbe(&g_env3Probe);
  appRegisterProbe(&g_envProProbe);
  appRegisterProbe(&g_soilMoistureProbe);
  appRegisterProbe(&g_dlightProbe);
  Serial.println("[atoms3_lite] setup: probes registered");

  AppUiHooks hooks{};
  hooks.onRgbVerifyStart = [](const String& sessionId, uint32_t confirmWindowMs, uint32_t /*startMs*/) {
    g_rgbVerifyPending = true;
    Serial.printf("rgb.verify.prompt.button session=%s windowMs=%lu\n",
                  sessionId.c_str(),
                  static_cast<unsigned long>(confirmWindowMs));
  };
  hooks.onRgbVerifyEnd = []() {
    g_rgbVerifyPending = false;
  };
  hooks.onLoop = []() {
    if (g_rgbVerifyPending && M5.BtnA.wasClicked()) {
      Serial.println("rgb.verify.user.confirmed.button");
      appOnRgbVerifyConfirm();
    }
  };

  Serial.println("[atoms3_lite] setup: calling appBegin");
  appBegin(hooks);
  Serial.println("[atoms3_lite] setup: appBegin returned");
}

void loop() {
  appUpdate();
}
