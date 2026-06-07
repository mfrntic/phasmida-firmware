// headless_stubs.cpp
// ------------------
// Empty stub implementations of UI screen and ScreenManager methods that are
// referenced by sensor probe code in core_common but never actually called at
// runtime (because all probes are constructed with nullptr screen pointers).
//
// These stubs satisfy the linker for the atoms3_lite headless build without
// pulling in the full M5GFX / display rendering stack.

#include <ui/DLightScreen.h>
#include <ui/EnvSensorScreen.h>
#include <ui/EnvProScreen.h>
#include <ui/SoilMoistureScreen.h>
#include <ui/ScreenManager.h>
#include <ui/IScreen.h>

// ── DLightScreen ──────────────────────────────────────────────────────────────
void DLightScreen::notifyNewReadings(float /*lux*/) {}

// ── EnvSensorScreen ───────────────────────────────────────────────────────────
void EnvSensorScreen::notifyNewReadings(float /*tempC*/, float /*humPct*/, float /*pressPa*/) {}

// ── EnvProScreen ──────────────────────────────────────────────────────────────
void EnvProScreen::notifyNewReadings(const SensorReading& /*data*/) {}

// ── SoilMoistureScreen ────────────────────────────────────────────────────────
void SoilMoistureScreen::notifyNewReadings(float /*moisturePct*/) {}

// ── ScreenManager ─────────────────────────────────────────────────────────────
void ScreenManager::setBootScreen(IScreen* /*screen*/) {}
bool ScreenManager::addScreen(IScreen* /*screen*/)                                        { return false; }
bool ScreenManager::addScreenBefore(IScreen* /*screen*/, IScreen* /*before*/)             { return false; }
bool ScreenManager::addScreenByPriority(IScreen* /*screen*/, int /*p*/, IScreen* /*a*/)   { return false; }
bool ScreenManager::removeScreen(IScreen* /*screen*/, bool /*animate*/)                   { return false; }
void ScreenManager::transitionFromBoot(bool /*animate*/) {}
void ScreenManager::showTransient(IScreen* /*screen*/) {}
void ScreenManager::dismissTransient() {}
void ScreenManager::setActiveIndex(int /*idx*/, bool /*animate*/) {}
void ScreenManager::handleTouch() {}
void ScreenManager::handleButtons() {}
void ScreenManager::update() {}
int  ScreenManager::activeIndex() const                                                    { return 0; }
int  ScreenManager::screenCount() const                                                    { return 0; }
bool ScreenManager::hasScreen(IScreen* /*screen*/) const                                   { return false; }
bool ScreenManager::isTransientActive() const                                              { return false; }
