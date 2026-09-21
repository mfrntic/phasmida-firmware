#include <sensors/SoilMoistureProbe.h>
#include <app_config.h>
#include <Arduino.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include <soc/soc_caps.h>

namespace {

// Presence check: with the internal pull-down enabled an unplugged Grove
// cable reads ~0, while a plugged Unit Earth reads well above this even with
// the probe in water (its on-board 10k pull-up vs. our ~45k pull-down gives
// >= ~1.4 V, raw >= ~1800). 200 raw ≈ 150 mV leaves a wide margin both ways.
constexpr int kPresenceRawThreshold = 200;

}  // namespace

SoilMoistureProbe::SoilMoistureProbe(SoilMoistureScreen* screen)
    : _screen(screen),
      _analogPin(AppConfig::kSoilMoistureAnalogPin),
      _digitalPin(AppConfig::kSoilMoistureDigitalPin),
      _lastRawValue(0),
      _isInitialized(false),
      _dryMv(AppConfig::kSoilMoistureDryMv),
      _wetMv(AppConfig::kSoilMoistureWetMv) {}

void SoilMoistureProbe::setCalibration(uint16_t dryMv, uint16_t wetMv) {
  _dryMv = dryMv;
  _wetMv = wetMv;
}

uint16_t SoilMoistureProbe::readMilliVolts() const {
  return static_cast<uint16_t>(analogReadMilliVolts(_analogPin));
}

const char* SoilMoistureProbe::name() const {
  return "SOIL";
}

const char* SoilMoistureProbe::telemetryType() const {
  return "soil-moisture";
}

bool SoilMoistureProbe::detect() {
  // A disconnected AOUT floats, so presence needs a defined idle level. The
  // obvious pinMode(INPUT_PULLDOWN) does not survive: arduino-esp32 2.x
  // analogRead()/analogReadMilliVolts() call pinMode(pin, ANALOG) on every
  // read, which clears the pull. And the pull must NOT be active during the
  // real measurement anyway — ~45k to GND in parallel with the probe would
  // drag the dry level from 3.3 V to ~2.7 V and break the mV calibration.
  //
  // So: take a dedicated presence sample with the pull-down enabled, reading
  // the ADC through the IDF driver (which leaves the pad config alone), then
  // release the pull-down so the next sample() sees the bare probe.
  analogRead(_analogPin);  // ensure ADC unit, attenuation and pad are set up
  int8_t channel = digitalPinToAnalogChannel(_analogPin);
  if (channel < 0 || channel >= SOC_ADC_MAX_CHANNEL_NUM) {
    // Not an ADC1 pin — no pull-down trick available, fall back to raw level.
    return analogRead(_analogPin) > kPresenceRawThreshold;
  }
  gpio_num_t gpio = static_cast<gpio_num_t>(_analogPin);
  gpio_pulldown_en(gpio);
  delayMicroseconds(200);  // let the pad settle against the pull-down
  int raw = adc1_get_raw(static_cast<adc1_channel_t>(channel));
  gpio_pulldown_dis(gpio);
  return raw > kPresenceRawThreshold;
}

bool SoilMoistureProbe::init() {
  // No pull on the analog pin — see detect(). DOUT is driven by the unit's
  // LM393 with an on-board 10k pull-up, so a plain input is enough.
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

  // No disconnect heuristic here: without a pull the floating input can read
  // anything. Hot-unplug is detected by ProbeRegistry via detect() instead.

  // Digital threshold output (HIGH = dry, i.e. above trim-pot threshold)
  bool isDry = digitalRead(_digitalPin) == HIGH;

  // Convert to percentage. The probe is resistive with a pull-up, so a high
  // voltage means DRY and a low voltage means WET — invert and scale between
  // the calibration points (defaults in app_config.h, per-pot values via
  // setCalibration), clamped to 0–100 %.
  // analogReadMilliVolts() applies the eFuse ADC calibration, which keeps the
  // numbers comparable to M5Stack's own driver and linearises the ADC ends.
  uint16_t mv = readMilliVolts();
  const float dry = _dryMv;
  const float wet = _wetMv;
  float moisturePercent = (dry - mv) / (dry - wet) * 100.0f;
  moisturePercent = constrain(moisturePercent, 0.0f, 100.0f);

  out = SensorReading{};
  out.hasSoilMoisture  = true;
  out.soilMoistureRaw  = raw;
  out.soilMoistureMv   = mv;
  out.soilMoisturePct  = moisturePercent;
  out.soilMoistureDry  = isDry;

  return true;
}
