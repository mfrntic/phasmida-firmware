#pragma once

#include <Arduino.h>

class TimeSync {
public:
  void     begin();
  bool     applyTimezone(const String& posixTz);
  void     clearTimezone();
  bool     sync(uint32_t timeoutMs);
  bool     isSynced()      const;
  bool     hasLocalTimezone() const;
  String   activeTimezone() const;
  uint64_t unixEpochMs()   const;

private:
  String _activeTimezone;
  bool   _hasLocalTimezone = false;
};
