#pragma once

#include <Arduino.h>

class DeviceIdentity {
public:
  void init();

  const String& macDisplay()     const { return _macDisplay; }
  const String& macSlug()        const { return _macSlug; }
  const String& mqttClientId()   const { return _mqttClientId; }
  const String& telemetryTopic() const { return _telemetryTopic; }
  const String& statusTopic()    const { return _statusTopic; }
  const String& eventsTopic()    const { return _eventsTopic; }
  const String& cmdTopic()       const { return _cmdTopic; }
  const String& cmdAckTopic()    const { return _cmdAckTopic; }

private:
  static String _slugFromMac(const String& mac);

  String _macDisplay;
  String _macSlug;
  String _mqttClientId;
  String _telemetryTopic;
  String _statusTopic;
  String _eventsTopic;
  String _cmdTopic;
  String _cmdAckTopic;
};
