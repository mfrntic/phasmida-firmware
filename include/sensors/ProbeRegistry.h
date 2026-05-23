#pragma once

#include <Arduino.h>
#include <functional>
#include <stddef.h>

#include "ISensorProbe.h"
#include "SensorReading.h"

class ScreenManager;
class IScreen;

// ProbeRegistry
// -------------
// Owns N independent sensor probes. All registered probes can run in
// parallel — every detected probe samples on its own cadence and emits
// readings independently. Adding a new probe type: implement ISensorProbe,
// then call registry.addProbe(&yourProbe) before begin().
//
// Hot-plug:
//   - On boot, detect() + init() each registered probe.
//   - During service(), if a present probe fails sample() N times in a row
//     it is shut down; detection is retried periodically.
//   - When a previously-absent probe appears, init() runs and its UI screens
//     are inserted into the attached ScreenManager.
//
// UI integration is opt-in via attachUi(). When attached, the registry adds
// each present probe's screens before `anchor` (or appends if no anchor),
// and removes them automatically when the probe disconnects.
class ProbeRegistry {
 public:
  using SampleCallback   = std::function<void(const SensorReading&, ISensorProbe*)>;
  using PresenceCallback = std::function<void(ISensorProbe*, bool present)>;

  // Register a probe driver. Pointer must outlive the registry; ownership
  // stays with caller. Order of registration controls detection order.
  bool addProbe(ISensorProbe* probe);

  // Optional: register a ScreenManager for automatic UI add/remove.
  // `anchor` is the screen that probe screens should be inserted before
  // (typically the Settings screen). Pass nullptr to append at the end.
  void attachUi(ScreenManager* mgr, IScreen* anchor = nullptr);

  // Detect + init all registered probes. Call once after attachUi().
  void begin();

  // Per-loop tick. Cheap when nothing to do.
  // Drives probe service() ticks and periodic sampling / hot-plug.
  void service();

  // Force a sampling pass right now, bypassing the throttle. Useful during
  // boot warm-up to let slow probes (e.g. BSEC) populate screens ASAP.
  void forceSampleNow() { _nextSampleAt = 0; service(); }

  // Optional callbacks (invoked from service() context only).
  void onSample(SampleCallback cb) { _sampleCb = std::move(cb); }
  void onPresenceChange(PresenceCallback cb) { _presenceCb = std::move(cb); }

  size_t        probeCount() const { return _count; }
  ISensorProbe* probeAt(size_t i) const { return i < _count ? _slots[i].probe : nullptr; }
  bool          isPresent(size_t i) const { return i < _count ? _slots[i].present : false; }

  // Most recent successful sample for probe `i`, if any. Returns false if the
  // probe never produced a reading or the cached value exceeds `maxAgeMs`.
  bool lastReading(size_t i, SensorReading& out, uint32_t maxAgeMs = 60000) const;

  // True if at least one registered probe is currently present.
  bool anyPresent() const;

 private:
  static constexpr size_t   kMaxProbes              = 8;
  static constexpr uint32_t kSampleIntervalMs       = 30000;
  // While any present probe still hasn't produced its first reading, sample
  // much more often so screens populate quickly after boot. Drops to the
  // steady-state cadence above as soon as every present probe has data.
  static constexpr uint32_t kWarmupSampleIntervalMs = 1000;
  static constexpr uint32_t kReinitBackoffMs        = 10000;
  // How often detect() is called on PRESENT probes to confirm they are still
  // physically connected. Independent of the sample cadence — ensures hot
  // unplug is observed within ~kPresenceCheckMs * kMissingDetectSamples,
  // not within the (much slower) telemetry interval.
  static constexpr uint32_t kPresenceCheckMs        = 2000;
  static constexpr uint8_t  kMissingConfirmSamples  = 3;
  static constexpr uint8_t  kMissingDetectSamples   = 3;

  struct Slot {
    ISensorProbe* probe         = nullptr;
    bool          present       = false;
    uint8_t       missingStreak = 0;
    uint8_t       absentDetectStreak = 0;
    uint32_t      nextReinitAt  = 0;
    uint32_t      nextPresenceCheckAt = 0;
    SensorReading lastReading{};
    uint32_t      lastReadingMs = 0;
    bool          hasReading    = false;
  };

  void _setPresent(Slot& slot, bool present);
  void _addProbeScreens(ISensorProbe* probe);
  void _removeProbeScreens(ISensorProbe* probe);
  int  _getProbeScreenPriority(ISensorProbe* probe) const;

  Slot     _slots[kMaxProbes];
  size_t   _count        = 0;
  uint32_t _nextSampleAt = 0;

  ScreenManager* _ui       = nullptr;
  IScreen*       _uiAnchor = nullptr;

  SampleCallback   _sampleCb;
  PresenceCallback _presenceCb;
};
