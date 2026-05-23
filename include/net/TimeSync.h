#pragma once

#include <Arduino.h>

class TimeSync {
public:
  void     begin();
  bool     sync(uint32_t timeoutMs);
  bool     isSynced()      const;
  uint64_t unixEpochMs()   const;
};
