#include <net/TimeSync.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

void TimeSync::begin() {
  clearTimezone();
  configTime(0, 0,
             "pool.ntp.org",
             "time.google.com",
             "time.windows.com");
}

bool TimeSync::applyTimezone(const String& posixTz) {
  if (posixTz.isEmpty()) {
    return false;
  }
  if (setenv("TZ", posixTz.c_str(), 1) != 0) {
    return false;
  }
  tzset();
  _activeTimezone = posixTz;
  _hasLocalTimezone = true;
  return true;
}

void TimeSync::clearTimezone() {
  setenv("TZ", "UTC0", 1);
  tzset();
  _activeTimezone = "";
  _hasLocalTimezone = false;
}

bool TimeSync::sync(uint32_t timeoutMs) {
  uint32_t startedAt = millis();
  while (!isSynced() && (millis() - startedAt) < timeoutMs) {
    delay(1000);
  }
  return isSynced();
}

bool TimeSync::isSynced() const {
  return unixEpochMs() > 1700000000000ULL;
}

bool TimeSync::hasLocalTimezone() const {
  return _hasLocalTimezone;
}

String TimeSync::activeTimezone() const {
  return _activeTimezone;
}

uint64_t TimeSync::unixEpochMs() const {
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) {
    return 0;
  }
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + (tv.tv_usec / 1000ULL);
}
