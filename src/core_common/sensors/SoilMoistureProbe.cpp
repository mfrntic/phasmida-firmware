#include <sensors/SoilMoistureProbe.h>
#include <ui/SoilMoistureScreen.h>
#include <app_config.h>
#include <Arduino.h>

SoilMoistureProbe::SoilMoistureProbe(SoilMoistureScreen* screen)
    : _screen(screen),
      _analogPin(AppConfig::kSoilMoistureAnalogPin),
      _digitalPin(AppConfig::kSoilMoistureDigitalPin),
      _lastRawValue(0),
      _isInitialized(false) {}

const char* SoilMoistureProbe::name() const {
  return "SOIL";
}

const char* SoilMoistureProbe::telemetryType() const {
  return "soil-moisture";
}

bool SoilMoistureProbe::detect() {
  // Ensure pull-down is active before reading — detect() is called before
  // init() by ProbeRegistry, so we must configure the pin here.
  // With INPUT_PULLDOWN, a disconnected pin is pulled to 0V and reads near 0.
  // A connected sensor always drives the line above ~200 raw.
  pinMode(_analogPin, INPUT_PULLDOWN);
  uint16_t raw = analogRead(_analogPin);
  return raw > 200;
}

bool SoilMoistureProbe::init() {
  // pinMode already set to INPUT_PULLDOWN in detect(); repeated here for
  // clarity and in case init() is ever called independently.
  pinMode(_analogPin, INPUT_PULLDOWN);
  pinMode(_digitalPin, INPUT);
  _isInitialized = true;
  return true;
}

bool SoilMoistureProbe::sample(SensorReading& out) {
  if (!_isInitialized) {
    return false;
  }

  uint16_t raw = analogRead(_analogPin);
  _lastRawValue = raw;

  // Threshold consistent with detect(): below 200 means pin is floating
  // (probe disconnected) — signal a missing sample so ProbeRegistry can
  // count it toward hot-unplug confirmation.
  if (raw <= 200) {
    return false;
  }

  // Digital threshold output (HIGH = dry, i.e. above trim-pot threshold)
  bool isDry = digitalRead(_digitalPin) == HIGH;

  // Convert to percentage: linear mapping from 0-4095 → 0-100%
  float moisturePercent = (raw / 4095.0f) * 100.0f;

  out = SensorReading{};
  out.hasSoilMoisture  = true;
  out.soilMoistureRaw  = raw;
  out.soilMoisturePct  = moisturePercent;
  out.soilMoistureDry  = isDry;

  return true;
}

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
