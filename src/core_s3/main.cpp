#include <Arduino.h>
#include <M5Unified.h>

#include <sensors/Env3Probe.h>
#include <sensors/EnvProProbe.h>
#include <sensors/SoilMoistureProbe.h>
#include <sensors/DLightProbe.h>
#include <sensors/ProbeRegistry.h>
#include <led/LedManager.h>
#include <led/RgbVerificationManager.h>
#include <ConfigStore.h>

#include <ui/BootLogScreen.h>
#include <ui/SoilMoistureScreen.h>
#include <ui/DLightScreen.h>
#include <ui/EnvSensorScreen.h>
#include <ui/EnvProScreen.h>
#include <ui/SettingsScreen.h>
#include <ui/ScreenManager.h>
#include <ui/splash_logo.h>
#include <ui/RgbVerificationScreen.h>
#include <ui/RgbLightScreen.h>

#include "../core_common/app_core.h"

namespace {

// UI subsystems
BootLogScreen   g_bootLogScreen;
ScreenManager   g_screenMgr;
SettingsScreen  g_settingsScreen(g_screenMgr, g_bootLogScreen);

EnvSensorScreen    g_env3BasicScreen("ENV 3");
EnvSensorScreen    g_envProBasicScreen("ENV PRO");
EnvProScreen       g_envProExtrasScreen;
SoilMoistureScreen g_soilMoistureScreen;
DLightScreen       g_dlightScreen;

RgbVerificationScreen g_rgbVerificationScreen;

// Sensor probes with screen pointers
Env3Probe         g_env3Probe(&g_env3BasicScreen);
EnvProProbe       g_envProProbe(&g_envProBasicScreen, &g_envProExtrasScreen);
SoilMoistureProbe g_soilMoistureProbe(&g_soilMoistureScreen);
DLightProbe       g_dlightProbe(&g_dlightScreen);

}  // namespace

// RgbLightScreen is allocated here (after anonymous namespace) because it
// requires references to LedManager and ConfigStore owned by app_core.
// It must outlive setup() so we use a global pointer with lazy init.
static RgbLightScreen* s_rgbLightScreen = nullptr;

void setup() {
  AppUiHooks hooks;

  hooks.onBootStart = []() {
    M5.Display.fillScreen(0x0000);
    M5.Display.drawJpg(kSplashLogoJpg, kSplashLogoJpgLen, 0, 0);
    delay(2500);
    g_screenMgr.setBootScreen(&g_bootLogScreen);
  };

  hooks.onBootLogLine = [](const String& line) {
    g_bootLogScreen.pushLine(line);
  };

  hooks.onUiSetupScreens = [](ProbeRegistry& probes) {
    g_screenMgr.addScreen(&g_settingsScreen);
    probes.attachUi(&g_screenMgr, &g_settingsScreen);
  };

  hooks.onUiPostProbeInit = []() {
    s_rgbLightScreen = new RgbLightScreen(appGetLedManager(), appGetConfigStore());
    g_screenMgr.addScreenByPriority(s_rgbLightScreen, 90, &g_settingsScreen);
  };

  hooks.onBootComplete = []() {
    g_screenMgr.transitionFromBoot();
  };

  hooks.onLoop = []() {
    g_screenMgr.handleTouch();
    g_screenMgr.handleButtons();
    g_screenMgr.update();
  };

  hooks.onRgbVerifyStart = [](const String& sessionId, uint32_t confirmWindowMs, uint32_t startMs) {
    g_rgbVerificationScreen.setSession(sessionId, confirmWindowMs, startMs);
    g_screenMgr.showTransient(&g_rgbVerificationScreen);
  };

  hooks.onRgbVerifyEnd = []() {
    g_screenMgr.dismissTransient();
  };

  appRegisterProbe(&g_env3Probe);
  appRegisterProbe(&g_envProProbe);
  appRegisterProbe(&g_soilMoistureProbe);
  appRegisterProbe(&g_dlightProbe);

  appBegin(hooks);

  g_rgbVerificationScreen.setConfirmCallback([]() { appOnRgbVerifyConfirm(); });
  g_rgbVerificationScreen.setRejectCallback([]()  { appOnRgbVerifyReject();  });
}

void loop() {
  appUpdate();
}
