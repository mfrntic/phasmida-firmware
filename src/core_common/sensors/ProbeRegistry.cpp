#include <sensors/ProbeRegistry.h>

#include <ui/IScreen.h>
#include <ui/ScreenManager.h>

bool ProbeRegistry::addProbe(ISensorProbe* probe) {
  if (!probe || _count >= kMaxProbes) {
    return false;
  }
  _slots[_count].probe = probe;
  ++_count;
  return true;
}

void ProbeRegistry::attachUi(ScreenManager* mgr, IScreen* anchor) {
  _ui       = mgr;
  _uiAnchor = anchor;
}

void ProbeRegistry::begin() {
  for (size_t i = 0; i < _count; ++i) {
    Slot& s = _slots[i];
    if (s.probe->detect() && s.probe->init()) {
      _setPresent(s, true);
    } else {
      s.nextReinitAt = millis() + kReinitBackoffMs;
    }
  }
  _nextSampleAt = millis();  // sample immediately on first service() so screens have data before transition
}

void ProbeRegistry::service() {
  // Tick each present probe every iteration (BSEC scheduler etc.).
  for (size_t i = 0; i < _count; ++i) {
    if (_slots[i].present) {
      _slots[i].probe->service();
    }
  }

  uint32_t now = millis();

  // Generic hot-plug presence loop, independent of the sample cadence:
  //   - ABSENT probes: poll detect() every kReinitBackoffMs. On success,
  //     init() and activate (UI screens added by _setPresent).
  //   - PRESENT probes: poll detect() every kPresenceCheckMs. After
  //     kMissingDetectSamples consecutive false results, declare the probe
  //     unplugged (shutdown + UI screens removed). This works uniformly for
  //     every ISensorProbe because they all already implement detect().
  for (size_t i = 0; i < _count; ++i) {
    Slot& s = _slots[i];
    if (!s.present) {
      if (now >= s.nextReinitAt) {
        s.nextReinitAt = now + kReinitBackoffMs;
        if (s.probe->detect() && s.probe->init()) {
          _setPresent(s, true);
        }
      }
    } else {
      if (now >= s.nextPresenceCheckAt) {
        s.nextPresenceCheckAt = now + kPresenceCheckMs;
        if (s.probe->detect()) {
          s.absentDetectStreak = 0;
        } else {
          if (s.absentDetectStreak < 255) ++s.absentDetectStreak;
          if (s.absentDetectStreak >= kMissingDetectSamples) {
            s.probe->shutdown();
            _setPresent(s, false);
            s.nextReinitAt = now + kReinitBackoffMs;
          }
        }
      }
    }
  }

  if (now < _nextSampleAt) {
    return;
  }

  // Decide next-sample cadence: fast warm-up until every present probe has
  // produced its first reading, then drop to the steady-state interval.
  bool anyMissingFirstReading = false;
  for (size_t i = 0; i < _count; ++i) {
    if (_slots[i].present && !_slots[i].hasReading) {
      anyMissingFirstReading = true;
      break;
    }
  }
  _nextSampleAt = now + (anyMissingFirstReading ? kWarmupSampleIntervalMs
                                                : kSampleIntervalMs);

  for (size_t i = 0; i < _count; ++i) {
    Slot& s = _slots[i];

    if (!s.present) continue;  // absent probes already handled above

    SensorReading reading{};
    bool ok = s.probe->sample(reading);
    if (ok) {
      s.missingStreak = 0;
      s.lastReading   = reading;
      s.lastReadingMs = now;
      s.hasReading    = true;
      s.probe->feedScreens(reading);
      if (_sampleCb) {
        _sampleCb(reading, s.probe);
      }
    } else {
      // Only treat repeated sample() failures as a real disconnect AFTER the
      // probe has produced at least one valid reading. Otherwise slow-start
      // probes (BSEC needs ~3 s) would be falsely declared missing during
      // boot warm-up and have their screens removed.
      if (!s.hasReading) {
        continue;
      }
      if (s.missingStreak < 255) ++s.missingStreak;
      if (s.missingStreak >= kMissingConfirmSamples) {
        s.probe->shutdown();
        _setPresent(s, false);
        s.nextReinitAt = now + kReinitBackoffMs;
      }
    }
  }
}

bool ProbeRegistry::anyPresent() const {
  for (size_t i = 0; i < _count; ++i) {
    if (_slots[i].present) return true;
  }
  return false;
}

bool ProbeRegistry::lastReading(size_t i, SensorReading& out, uint32_t maxAgeMs) const {
  if (i >= _count) return false;
  const Slot& s = _slots[i];
  if (!s.hasReading) return false;
  if (millis() - s.lastReadingMs > maxAgeMs) return false;
  out = s.lastReading;
  return true;
}

void ProbeRegistry::_setPresent(Slot& slot, bool present) {
  if (slot.present == present) return;
  slot.present       = present;
  slot.missingStreak = 0;
  slot.absentDetectStreak = 0;
  // Reset hasReading on reconnect so the warm-up cadence kicks in and the
  // screen is populated quickly without waiting for the next 30s sample tick.
  if (present) {
    slot.hasReading = false;
    slot.nextPresenceCheckAt = millis() + kPresenceCheckMs;
  }

  if (present) {
    _addProbeScreens(slot.probe);
  } else {
    _removeProbeScreens(slot.probe);
  }
  if (_presenceCb) {
    _presenceCb(slot.probe, present);
  }
}

void ProbeRegistry::_addProbeScreens(ISensorProbe* probe) {
  if (!_ui) return;
  int priority = _getProbeScreenPriority(probe);
  size_t n = probe->screenCount();
  for (size_t i = 0; i < n; ++i) {
    IScreen* s = probe->screen(i);
    if (!s) continue;
    if (_uiAnchor && _ui->hasScreen(_uiAnchor)) {
      _ui->addScreenByPriority(s, priority, _uiAnchor);
    } else {
      _ui->addScreen(s);
    }
  }
}

void ProbeRegistry::_removeProbeScreens(ISensorProbe* probe) {
  if (!_ui) return;
  size_t n = probe->screenCount();
  for (size_t i = 0; i < n; ++i) {
    IScreen* s = probe->screen(i);
    if (s) _ui->removeScreen(s, true);
  }
}

int ProbeRegistry::_getProbeScreenPriority(ISensorProbe* probe) const {
  // Fixed carousel order for probe-owned screens:
  // ENV III (0), ENV PRO (1), SOIL (2), LIGHT (3), then non-probe screens.
  const char* telemetry = probe->telemetryType();
  if (telemetry) {
    if (strcmp(telemetry, "env-iii") == 0) {
      return 0;  // ENV III
    }
    if (strcmp(telemetry, "env-pro") == 0) {
      return 1;  // ENV PRO
    }
    if (strcmp(telemetry, "soil-moisture") == 0) {
      return 2;  // SOIL
    }
    if (strcmp(telemetry, "light") == 0) {
      return 3;  // LIGHT
    }
  }
  return 80;  // Unknown probes stay after known probes, before RGB/Settings.
}
