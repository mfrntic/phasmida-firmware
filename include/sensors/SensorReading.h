#pragma once

#include <stdint.h>

// Generic envelope for any environmental measurement produced by a probe.
// Each driver fills only the fields it can actually produce and sets the
// matching has* flag. Consumers (telemetry, UI, logging) MUST check the
// has* flag before reading a value.
//
// Adding a new metric (e.g. soil pH, CO2, particulate matter):
//   1. Append `bool hasX` + value field(s) here.
//   2. Set them in the new probe driver's sample() implementation.
//   3. Optionally add the metric to telemetry serialization.
//
// Kept value-only and POD-friendly so it can be copied cheaply between
// the BSEC callback context and the main loop.
struct SensorReading {
  // Atmospheric (ENV 3, ENV PRO, ...)
  bool  hasTemperature = false;
  float temperatureC   = 0.0f;

  bool  hasHumidity = false;
  float humidityPct = 0.0f;

  bool  hasPressure = false;
  float pressurePa  = 0.0f;

  // Air quality (ENV PRO via BSEC2)
  bool    hasGas           = false;
  float   gasResistanceOhm = 0.0f;

  bool    hasIaq      = false;
  float   iaq         = 0.0f;
  uint8_t iaqAccuracy = 0;

  // Soil (Earth Unit)
  bool     hasSoilMoisture    = false;
  uint16_t soilMoistureRaw    = 0;      // raw ADC 0–4095
  float    soilMoisturePct    = 0.0f;   // linear 0–100 %
  bool     soilMoistureDry    = false;  // DOUT: true = dry (above trim-pot threshold)

  // Light (Light/LUX Unit, planned)
  bool  hasLux = false;
  float lux    = 0.0f;
};
