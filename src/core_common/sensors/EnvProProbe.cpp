#include <sensors/EnvProProbe.h>

#include <Wire.h>

#include <app_config.h>
#include <ui/EnvProScreen.h>
#include <ui/EnvSensorScreen.h>

namespace {
constexpr uint8_t  kBme688Addr    = 0x77;
constexpr uint32_t kStaleSampleMs = 30000;

bool i2cPing(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}
}  // namespace

EnvProProbe* EnvProProbe::s_instance = nullptr;

EnvProProbe::EnvProProbe(EnvSensorScreen* basicScreen, EnvProScreen* proScreen)
    : _basic(basicScreen), _pro(proScreen) {
  s_instance = this;  // single-instance bridge (BSEC2 callback has no userdata)
}

bool EnvProProbe::detect() {
  Wire.begin(AppConfig::kI2cSda, AppConfig::kI2cScl, AppConfig::kI2cFreq);
  return i2cPing(kBme688Addr);
}

bool EnvProProbe::init() {
  _ready        = false;
  _lastSampleMs = 0;
  _hasTemp = _hasHum = _hasPress = false;
  _latest = SensorReading{};

  if (!_bsec.begin(kBme688Addr, Wire)) {
    return false;
  }

  bsecSensor sensorList[] = {
      BSEC_OUTPUT_IAQ,
      BSEC_OUTPUT_RAW_TEMPERATURE,
      BSEC_OUTPUT_RAW_PRESSURE,
      BSEC_OUTPUT_RAW_HUMIDITY,
      BSEC_OUTPUT_RAW_GAS,
      BSEC_OUTPUT_STABILIZATION_STATUS,
      BSEC_OUTPUT_RUN_IN_STATUS,
  };
  // LP profile (3 s) matches official ENV_PRO example and IAQ requirements.
  _bsec.updateSubscription(sensorList,
                           sizeof(sensorList) / sizeof(sensorList[0]),
                           BSEC_SAMPLE_RATE_LP);
  if (_bsec.status < BSEC_OK) {
    return false;  // hard error; warnings (>= BSEC_OK) are tolerated
  }

  _bsec.attachCallback(&EnvProProbe::_bsecBridge);
  return true;
}

void EnvProProbe::service() {
  // BSEC scheduler must tick every loop iteration to honour LP profile timing.
  _bsec.run();
}

void EnvProProbe::shutdown() {
  _ready        = false;
  _lastSampleMs = 0;
}

bool EnvProProbe::sample(SensorReading& out) {
  if (!_ready) return false;
  if (millis() - _lastSampleMs > kStaleSampleMs) return false;
  out = _latest;
  return true;
}

size_t EnvProProbe::screenCount() const {
  size_t n = 0;
  if (_basic) ++n;
  if (_pro) ++n;
  return n;
}

IScreen* EnvProProbe::screen(size_t idx) const {
  if (_basic && idx == 0) return _basic;
  if (_pro && idx == (_basic ? 1u : 0u)) return _pro;
  return nullptr;
}

void EnvProProbe::feedScreens(const SensorReading& r) {
  if (_basic) {
    _basic->notifyNewReadings(r.temperatureC, r.humidityPct, r.pressurePa);
  }
  if (_pro) {
    _pro->notifyNewReadings(r);
  }
}

void EnvProProbe::_bsecBridge(const bme68xData /*data*/,
                              const bsecOutputs outputs,
                              const Bsec2 /*bsec*/) {
  if (s_instance) s_instance->_onBsecOutputs(outputs);
}

void EnvProProbe::_onBsecOutputs(const bsecOutputs& outputs) {
  if (!outputs.nOutputs) return;

  SensorReading next = _latest;
  for (uint8_t i = 0; i < outputs.nOutputs; ++i) {
    const bsecData o = outputs.output[i];
    switch (o.sensor_id) {
      case BSEC_OUTPUT_RAW_TEMPERATURE:
        next.temperatureC = o.signal;
        next.hasTemperature = true;
        _hasTemp = true;
        break;
      case BSEC_OUTPUT_RAW_HUMIDITY:
        next.humidityPct = o.signal;
        next.hasHumidity = true;
        _hasHum = true;
        break;
      case BSEC_OUTPUT_RAW_PRESSURE:
        // BSEC2 wrapper exposes RAW_PRESSURE in hPa on this build.
        next.pressurePa = o.signal * 100.0f;
        next.hasPressure = true;
        _hasPress = true;
        break;
      case BSEC_OUTPUT_RAW_GAS:
        next.gasResistanceOhm = o.signal;
        next.hasGas = true;
        break;
      case BSEC_OUTPUT_IAQ:
        next.iaq         = o.signal;
        next.iaqAccuracy = o.accuracy;
        next.hasIaq      = true;
        break;
      default:
        break;
    }
  }

  if (!_hasTemp || !_hasHum || !_hasPress) return;
  if (next.temperatureC < -40.0f   || next.temperatureC > 85.0f)     return;
  if (next.humidityPct  <   0.0f   || next.humidityPct  > 100.0f)    return;
  if (next.pressurePa   < 30000.0f || next.pressurePa   > 120000.0f) return;

  _latest       = next;
  _ready        = true;
  _lastSampleMs = millis();
}
