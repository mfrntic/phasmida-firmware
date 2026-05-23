#include <net/MqttClient.h>
#include <camera_config.h>
#include <WiFi.h>

MqttClient* MqttClient::_instance = nullptr;

namespace {

void logDnsFailureDiag(const String& host) {
  Serial.printf(
      "[MQTT][DNS] host=%s status=%d ip=%s gw=%s dns0=%s dns1=%s rssi=%d\n",
      host.c_str(),
      WiFi.status(),
      WiFi.localIP().toString().c_str(),
      WiFi.gatewayIP().toString().c_str(),
      WiFi.dnsIP(0).toString().c_str(),
      WiFi.dnsIP(1).toString().c_str(),
      WiFi.RSSI());
}

}  // namespace

MqttClient::MqttClient() : _client(_wifi) {}

void MqttClient::_rawCallback(char* topic, byte* payload, unsigned int length) {
  if (_instance) _instance->_handleMessage(topic, payload, length);
}

void MqttClient::_handleMessage(char* topic, byte* payload, unsigned int length) {
  if (!_onMessage) return;
  String payloadStr;
  payloadStr.reserve(length + 1);
  for (unsigned int i = 0; i < length; ++i) {
    payloadStr += static_cast<char>(payload[i]);
  }
  _onMessage(String(topic), payloadStr);
}

void MqttClient::begin(const MqttConfig& cfg) {
  _cfg = cfg;
  _reconnectDelayMs = CamConfig::kMqttReconnectBaseDelayMs;
  _nextReconnectAt  = 0;
  _instance         = this;

  if (_cfg.port == 8883) {
    _wifiSecure.setInsecure();
    _client.setClient(_wifiSecure);
  } else {
    _client.setClient(_wifi);
  }

  _client.setServer(_cfg.host.c_str(), _cfg.port);
  _client.setCallback(_rawCallback);
  _client.setBufferSize(_cfg.bufferSize);
  _client.setSocketTimeout(_cfg.socketTimeoutSec);
  _client.setKeepAlive(_cfg.keepAliveSec);
}

bool MqttClient::connect() {
  if (WiFi.status() != WL_CONNECTED) {
    _scheduleReconnect(false);
    return false;
  }

  IPAddress mqttIp;
  if (!WiFi.hostByName(_cfg.host.c_str(), mqttIp)) {
    logDnsFailureDiag(_cfg.host);
    if (_onStatusChange) _onStatusChange("dns_failed", _cfg.host.c_str());
    _scheduleReconnect(false);
    return false;
  }

  WiFiClient tcpProbe;
  tcpProbe.setTimeout(1500);
  if (!tcpProbe.connect(mqttIp, _cfg.port)) {
    String ipStr = mqttIp.toString();
    if (_onStatusChange) _onStatusChange("tcp_failed", ipStr.c_str());
    _scheduleReconnect(false);
    return false;
  }
  tcpProbe.stop();

  bool connected = _client.connect(
      _cfg.clientId.c_str(),
      _cfg.username.c_str(),
      _cfg.password.c_str(),
      _cfg.statusTopicForLwt.c_str(),
      1,
      _cfg.willRetained,
      _cfg.willPayload.c_str());

  if (connected) {
    _reconnectDelayMs = CamConfig::kMqttReconnectBaseDelayMs;
    String ipStr = mqttIp.toString();
    if (_onStatusChange) _onStatusChange("connected", ipStr.c_str());
    return true;
  }

  int rc = _client.state();
  char rcStr[8];
  snprintf(rcStr, sizeof(rcStr), "%d", rc);
  if (_onStatusChange) _onStatusChange("connect_failed", rcStr);
  _scheduleReconnect(rc == 4 || rc == 5);
  return false;
}

bool MqttClient::isConnected() {
  return _client.connected();
}

void MqttClient::loop() {
  if (_client.connected()) {
    _client.loop();
    return;
  }
  if (millis() >= _nextReconnectAt) {
    connect();
  }
}

bool MqttClient::publish(const String& topic, const String& payload, bool retained) {
  return _client.publish(topic.c_str(), payload.c_str(), retained);
}

bool MqttClient::subscribe(const String& topic, uint8_t qos) {
  return _client.subscribe(topic.c_str(), qos);
}

void MqttClient::onMessage(MessageCallback cb) {
  _onMessage = cb;
}

void MqttClient::onStatusChange(StatusCallback cb) {
  _onStatusChange = cb;
}

uint32_t MqttClient::_addJitter(uint32_t baseDelayMs) {
  uint32_t jitter = baseDelayMs / 5U;
  int32_t delta = static_cast<int32_t>(random(-static_cast<int32_t>(jitter), static_cast<int32_t>(jitter) + 1));
  int32_t adjusted = static_cast<int32_t>(baseDelayMs) + delta;
  return static_cast<uint32_t>(max(1000, adjusted));
}

void MqttClient::_scheduleReconnect(bool authFailure) {
  if (authFailure) {
    _reconnectDelayMs = max(_reconnectDelayMs, CamConfig::kMqttAuthFailureInitialDelayMs);
  }
  uint32_t nextDelay = _addJitter(_reconnectDelayMs);
  _nextReconnectAt   = millis() + nextDelay;
  _reconnectDelayMs  = min(_reconnectDelayMs * 2U, CamConfig::kMqttReconnectMaxDelayMs);
  if (_onStatusChange) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%lu ms", static_cast<unsigned long>(nextDelay));
    _onStatusChange("reconnect_scheduled", buf);
  }
}
