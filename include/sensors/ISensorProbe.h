#pragma once

#include <Arduino.h>
#include <stddef.h>

#include "SensorReading.h"

class IScreen;

// ISensorProbe
// ------------
// Abstract driver for any sensor unit attached to the device.
// One concrete class per physical unit (ENV 3, ENV PRO, Earth, Light, ...).
// All probes share the same I2C bus; the registry orchestrates detection,
// initialization, periodic sampling, and hot-plug recovery.
//
// Lifecycle (driven by ProbeRegistry):
//   begin()        => detect() then init() if detected
//   service()      => called every loop iteration while the probe is present
//   sample()       => called periodically; non-fresh data should return false
//   shutdown()     => called when the probe is declared missing (hot-unplug)
//
// UI integration:
//   The probe owns its own screen instance(s) and pushes data into them via
//   feedScreens(). The registry will call screenCount()/screen(i) to register
//   them with the ScreenManager when the probe becomes present, and remove
//   them when the probe disappears.
class ISensorProbe {
 public:
  virtual ~ISensorProbe() = default;

  // Human-readable identifier used for logging and UI labels.
  virtual const char* name() const = 0;

  // Stable protocol identifier used as `sensorType` in MQTT telemetry payloads.
  // Must be lowercase, hyphen-separated, and must NOT change after deployment
  // because the backend uses it to distinguish sensor profiles.
  // Examples: "env-iii", "env-pro", "earth", "light"
  virtual const char* telemetryType() const = 0;

  // Cheap presence check. Implementation should be a non-destructive I2C ping.
  // Called both at boot and periodically during hot-plug polling.
  virtual bool detect() = 0;

  // Configure the hardware so subsequent sample() calls work.
  // May perform multi-step initialization (subscriptions, calibration, ...).
  virtual bool init() = 0;

  // Optional cleanup when the probe is hot-unplugged. Default: no-op.
  virtual void shutdown() {}

  // Called every loop iteration while the probe is present.
  // Useful for callback-driven libraries (e.g. BSEC2 scheduler tick).
  virtual void service() {}

  // Copy the latest valid reading into `out`. Return false if no fresh data
  // is available (will be treated as a missed sample for hot-plug logic).
  virtual bool sample(SensorReading& out) = 0;

  // Number of UI screens owned by this probe (0 if probe has no UI).
  virtual size_t screenCount() const { return 0; }

  // Get screen at index in [0, screenCount()).
  virtual IScreen* screen(size_t /*idx*/) const { return nullptr; }

  // Push the most recent sample into this probe's screen(s).
  // Default: no-op (probes without UI, or those that update screens via
  // an internal callback, can ignore this).
  virtual void feedScreens(const SensorReading& /*reading*/) {}
};
