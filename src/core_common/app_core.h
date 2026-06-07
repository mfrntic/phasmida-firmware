#pragma once

#include <Arduino.h>
#include <functional>

// AppUiHooks — optional callbacks that the headful main.cpp fills in.
// Headless targets pass a default-constructed (empty) AppUiHooks{}.
struct AppUiHooks {
  // Called during setup(), before boot log starts — display splash screen here.
  std::function<void()> onBootStart;

  // Receives each boot log line so the headful target can push it to BootLogScreen.
  std::function<void(const String&)> onBootLogLine;

  // Called after runtime config is loaded and before probes are initialised —
  // set up ScreenManager, add SettingsScreen, call probes.attachUi() here.
  // Parameter: reference to the ProbeRegistry so the caller can call attachUi().
  std::function<void(class ProbeRegistry&)> onUiSetupScreens;

  // Called after probes.begin() completes — add the RgbLightScreen here.
  std::function<void()> onUiPostProbeInit;

  // Called at the end of setup(), after all subsystems are initialised —
  // start the screen carousel here.
  std::function<void()> onBootComplete;

  // Called every loop() iteration — handle touch, buttons, screen updates here.
  std::function<void()> onLoop;

  // Called when an RGB verification session starts — show the prompt screen.
  // Parameters: sessionId, confirmWindowMs, startMs
  std::function<void(const String&, uint32_t, uint32_t)> onRgbVerifyStart;

  // Called when an RGB verification session ends (confirmed / rejected / timeout).
  std::function<void()> onRgbVerifyEnd;
};

// Register sensor probes before calling appBegin().
// Must be called for each probe in the desired carousel order.
void appRegisterProbe(class ISensorProbe* probe);

// Main firmware entry points — call from setup() / loop() in main.cpp.
void appBegin(const AppUiHooks& hooks);
void appUpdate();

// Accessors used by the headful main.cpp to wire up screen objects.
// Returns a reference to the LedManager instance owned by app_core.
class LedManager&    appGetLedManager();
// Returns a reference to the ConfigStore instance owned by app_core.
class ConfigStore&   appGetConfigStore();

// Called by headful RgbVerificationScreen confirm/reject buttons.
void appOnRgbVerifyConfirm();
void appOnRgbVerifyReject();
