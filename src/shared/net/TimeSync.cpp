#include <net/TimeSync.h>
#include <app_config.h>
#include <sys/time.h>

void TimeSync::begin() {
  configTime(0, 0, "pool.ntp.org", "time.google.com", "time.windows.com");
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

uint64_t TimeSync::unixEpochMs() const {
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) {
    return 0;
  }
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL + (tv.tv_usec / 1000ULL);
}
