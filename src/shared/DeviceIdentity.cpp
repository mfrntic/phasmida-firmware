#include <DeviceIdentity.h>
#include <WiFi.h>
#include <cctype>

void DeviceIdentity::init() {
  _macDisplay = WiFi.macAddress();
  _macSlug    = _slugFromMac(_macDisplay);

  _mqttClientId   = String("phasmida-") + _macSlug;
  _telemetryTopic = String("phasmida/") + _macSlug + "/telemetry";
  _statusTopic    = String("phasmida/") + _macSlug + "/status";
  _eventsTopic    = String("phasmida/") + _macSlug + "/events";
  _cmdTopic       = String("phasmida/") + _macSlug + "/cmd";
  _cmdAckTopic    = String("phasmida/") + _macSlug + "/cmd/ack";
}

String DeviceIdentity::_slugFromMac(const String& mac) {
  String slug;
  slug.reserve(12);
  for (size_t i = 0; i < mac.length(); ++i) {
    char c = mac.charAt(i);
    if (c == ':') {
      continue;
    }
    slug += static_cast<char>(tolower(static_cast<unsigned char>(c)));
  }
  return slug;
}
