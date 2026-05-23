#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <functional>

struct MqttConfig {
  String   host;
  uint16_t port;
  String   clientId;
  String   username;
  String   password;
  String   statusTopicForLwt;
  String   willPayload;
  bool     willRetained     = true;
  uint16_t keepAliveSec     = 60;
  uint16_t bufferSize       = 1024;
  uint8_t  socketTimeoutSec = 5;
};

class MqttClient {
public:
  using MessageCallback = std::function<void(const String& topic, const String& payload)>;
  using StatusCallback  = std::function<void(const char* event, const char* detail)>;

  MqttClient();
  void begin(const MqttConfig& cfg);

  bool connect();
  bool isConnected();
  void loop();

  bool publish(const String& topic, const String& payload, bool retained = false);
  bool subscribe(const String& topic, uint8_t qos = 1);

  void onMessage(MessageCallback cb);
  void onStatusChange(StatusCallback cb);

private:
  WiFiClientSecure _wifi;
  PubSubClient  _client;
  MqttConfig      _cfg;
  uint32_t        _reconnectDelayMs = 0;
  uint32_t        _nextReconnectAt  = 0;
  MessageCallback _onMessage;
  StatusCallback  _onStatusChange;

  void     _scheduleReconnect(bool authFailure);
  uint32_t _addJitter(uint32_t baseDelayMs);
  void     _handleMessage(char* topic, byte* payload, unsigned int length);

  static MqttClient* _instance;
  static void        _rawCallback(char* topic, byte* payload, unsigned int length);
};
